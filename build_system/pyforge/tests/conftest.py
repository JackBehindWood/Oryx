import hashlib
import importlib
import os
import pkgutil
import platform
import sys
import sysconfig
import tomllib
from pathlib import Path

import pytest
from typer.testing import CliRunner

REAL_ROOT = Path(__file__).resolve().parents[3]

# build_system/oryx/ (the Oryx plugin, see forge.toml's [plugins] paths) never moved into pyforge
# and isn't installed as its own package — it's importable as a namespace-package submodule of
# pyforge as long as the repo root is on sys.path, exactly like pyforge's own plugin loader
# (pyforge/pyforge/src/pyforge/plugins.py::_import_plugin) arranges for a real `forge` run.
if str(REAL_ROOT) not in sys.path:
    sys.path.insert(0, str(REAL_ROOT))

import pyforge
from pyforge.config import RunContext, parse_config
from pyforge.project import Project

GITMODULES = """\
[submodule "tests/vendor/doctest"]
\tpath = tests/vendor/doctest
\turl = https://github.com/doctest/doctest.git
[submodule "Oryx/vendor/spdlog"]
\tpath = Oryx/vendor/spdlog
\turl = https://github.com/gabime/spdlog.git
[submodule "Oryx/vendor/pybind11"]
\tpath = Oryx/vendor/pybind11
\turl = https://github.com/pybind/pybind11.git
[submodule "Oryx/vendor/yaml-cpp"]
\tpath = Oryx/vendor/yaml-cpp
\turl = https://github.com/jbeder/yaml-cpp.git
"""

FORGE_TOML = """\
[project]
name = "Oryx"
default-target = "oasis"

[build]
dependencies-dir = "Oryx/vendor"
fetch = "never"

[options]
python = { default = true, off = "--no-python" }
sanitize = { default = false, on = "--sanitize" }

[targets.oasis]
project = "Oasis"
presets.bench = ["--simulate=random,first-legal,100", "--benchmark"]

[tests]
project = "Tests"
suites.unit = "tests/unit"
suites.integration = "tests/integration"
suites.benchmark = { dir = "tests/benchmark", default = false }

[dependencies]
spdlog = { kind = "static", include = "include", sources = "src", defines = ["SPDLOG_COMPILED_LIB"] }
yaml-cpp = { kind = "static", include = "include", sources = "src" }
pybind11 = { include = "include", requires = ["python"] }
doctest = { include = "doctest", path = "tests/vendor/doctest" }

[docs]
tool = "mkdocs"

[plugins]
paths = ["build_system/oryx"]

[tool.oryx]
stubs-dir = "OryxPython/stubs"
"""

VENDOR_DIRS = ["Oryx/vendor/spdlog", "Oryx/vendor/pybind11", "Oryx/vendor/yaml-cpp", "tests/vendor/doctest"]


FIXTURES = Path(__file__).parent / "fixtures"


def workspace_json(root: Path) -> str:
    return (FIXTURES / "workspace.json").read_text(encoding="utf-8").replace("{root}", root.as_posix())


def write_workspace(root: Path) -> Path:
    path = root / "build" / "forge" / "workspace.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(workspace_json(root), encoding="utf-8")
    return path


def make_file(project: str) -> str:
    blocks = []
    for index, (token, outputdir) in enumerate(
        [
            ("debug_x64", "Debug-linux-x86_64"),
            ("debug_arm64", "Debug-linux-AARCH64"),
            ("release_x64", "Release-linux-x86_64"),
        ]
    ):
        keyword = "ifeq" if index == 0 else "else ifeq"
        blocks.append(
            f"{keyword} ($(config),{token})\n"
            f"TARGETDIR = bin/{outputdir}/{project}\n"
            f"TARGET = $(TARGETDIR)/{project}\n"
            f"OBJDIR = bin-int/{outputdir}/{project}\n"
        )
    return f"ifndef config\n  config=debug_x64\nendif\n\n{''.join(blocks)}endif\n"


def _protected_files() -> list[Path]:
    files = [REAL_ROOT / "oryx.local.toml", REAL_ROOT / "forge.local.toml"]
    files += sorted((REAL_ROOT / ".vscode").rglob("*"))
    files.append(Path(sysconfig.get_path("purelib")) / "oryx_research_host.pth")
    return files


def _snapshot() -> dict[str, str | None]:
    return {
        str(path): hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        for path in _protected_files()
    }


@pytest.fixture(autouse=True)
def no_network(monkeypatch):
    import urllib.request

    def refuse(*args, **kwargs):
        raise AssertionError("tests must not download anything")

    monkeypatch.setattr(urllib.request, "urlretrieve", refuse)
    monkeypatch.setattr(urllib.request, "urlopen", refuse)


@pytest.fixture(scope="session", autouse=True)
def real_dev_files_untouched():
    before = _snapshot()
    yield
    assert _snapshot() == before, "a test touched real developer files (local config, .vscode or the venv .pth)"


@pytest.fixture(scope="session", autouse=True)
def _load_forge_app():
    """Import pyforge.main once, from the real repo root, before any test's tmp_project chdir
    runs — this is what mounts the oryx plugin's commands (see pyforge/main.py), and it must
    happen against the real forge.toml so it's independent of test order."""
    import pyforge.main  # noqa: F401


def _patchable_modules() -> list:
    for info in pkgutil.walk_packages(pyforge.__path__, prefix="pyforge."):
        if not info.name.startswith("pyforge.tests"):
            importlib.import_module(info.name)
    import build_system.oryx

    for info in pkgutil.walk_packages(build_system.oryx.__path__, prefix="build_system.oryx."):
        importlib.import_module(info.name)
    return [
        module
        for name, module in sys.modules.items()
        if (name.split(".")[0] == "pyforge" or name.startswith("build_system.oryx")) and ".tests" not in name
    ]


def patch_everywhere(monkeypatch, name: str, value) -> None:
    for module in _patchable_modules():
        if name in vars(module):
            monkeypatch.setattr(module, name, value)


@pytest.fixture
def linux_host(monkeypatch):
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    monkeypatch.setattr(platform, "machine", lambda: "x86_64")
    monkeypatch.setattr(os, "cpu_count", lambda: 8)


@pytest.fixture
def fake_python(monkeypatch):
    from build_system.oryx.python_env import PythonBuildInfo

    info = PythonBuildInfo(
        include_dir=Path("/py/include/python3.11"),
        lib_dir=Path("/py/lib"),
        lib_name="python3.11",
        home=Path("/py"),
        site_packages=(Path("/venv/lib/python3.11/site-packages"),),
    )
    patch_everywhere(monkeypatch, "python_build_info", lambda: info)
    return info


@pytest.fixture
def tmp_project(tmp_path, monkeypatch, linux_host) -> Path:
    monkeypatch.chdir(tmp_path)
    # Isolates cache.user_cache_dir() from the real machine's ~/.cache (or platform
    # equivalent), so tests never read or write a developer's actual Premake cache.
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    (tmp_path / ".gitmodules").write_text(GITMODULES, encoding="utf-8")
    for vendor_dir in VENDOR_DIRS:
        (tmp_path / vendor_dir).mkdir(parents=True)
    (tmp_path / "forge.toml").write_text(FORGE_TOML, encoding="utf-8")
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    for project in ("Oryx", "Tests"):
        (build_dir / f"{project}.make").write_text(make_file(project), encoding="utf-8")
    write_workspace(tmp_path)
    return tmp_path


@pytest.fixture
def forge(tmp_project, fake_python):
    from pyforge.main import app

    runner = CliRunner()

    def invoke(*args: str):
        return runner.invoke(app, list(args), env={"COLUMNS": "10000"})

    return invoke


@pytest.fixture
def project(tmp_project) -> Project:
    return Project.discover(tmp_project)


def make_run(project: Project, profile: str = "debug", **options: bool) -> RunContext:
    cfg = parse_config(tomllib.loads(project.config_file.read_text(encoding="utf-8")))
    values = {name: spec.default for name, spec in cfg.options.items()} | options
    return RunContext(project=project, config=cfg, profile=profile, options=values)


@pytest.fixture
def run(project) -> RunContext:
    return make_run(project)
