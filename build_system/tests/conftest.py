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

import build_system
from build_system.config import RunContext, parse_config
from build_system.project import Project

REAL_ROOT = Path(__file__).resolve().parents[2]

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

[options]
python = { default = true, off = "--no-python" }
sanitize = { default = false, on = "--sanitize" }

[targets.oasis]
project = "Oasis"

[tests]
project = "Tests"

[docs]
tool = "mkdocs"

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


@pytest.fixture(scope="session", autouse=True)
def real_dev_files_untouched():
    before = _snapshot()
    yield
    assert _snapshot() == before, "a test touched real developer files (local config, .vscode or the venv .pth)"


def _build_system_modules() -> list:
    for info in pkgutil.walk_packages(build_system.__path__, prefix="build_system."):
        if not info.name.startswith("build_system.tests"):
            importlib.import_module(info.name)
    return [module for name, module in sys.modules.items() if name.split(".")[0] == "build_system" and ".tests" not in name]


def patch_everywhere(monkeypatch, name: str, value) -> None:
    for module in _build_system_modules():
        if name in vars(module):
            monkeypatch.setattr(module, name, value)


@pytest.fixture
def linux_host(monkeypatch):
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    monkeypatch.setattr(platform, "machine", lambda: "x86_64")
    monkeypatch.setattr(os, "cpu_count", lambda: 8)


@pytest.fixture
def fake_python(monkeypatch):
    from build_system.setup.python_env import PythonBuildInfo

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
    from build_system.main import app

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
