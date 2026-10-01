import functools
import hashlib
import http.server
import importlib
import io
import os
import pkgutil
import platform
import shutil
import subprocess
import sys
import tarfile
import threading
import tomllib
from pathlib import Path

import pytest
from typer.testing import CliRunner

REAL_ROOT = Path(__file__).resolve().parents[3]

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
paths = ["dummy_plugin"]

[tool.dummy]
greeting = "hello from the dummy plugin"
"""

VENDOR_DIRS = ["Oryx/vendor/spdlog", "Oryx/vendor/pybind11", "Oryx/vendor/yaml-cpp", "tests/vendor/doctest"]


FIXTURES = Path(__file__).parent / "fixtures"
DUMMY_PREMAKE_ARGS = "--dummy-flag=1"


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
    files = [REAL_ROOT / "forge.local.toml"]
    files += sorted((REAL_ROOT / ".vscode").rglob("*"))
    return files


def _snapshot() -> dict[str, str | None]:
    return {
        str(path): hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        for path in _protected_files()
    }


@pytest.fixture(autouse=True)
def no_network(monkeypatch):
    import urllib.request

    def guard(real):
        def call(url, *args, **kwargs):
            if not str(getattr(url, "full_url", url)).startswith("http://127.0.0.1:"):
                raise AssertionError("tests must not download anything")
            return real(url, *args, **kwargs)

        return call

    monkeypatch.setattr(urllib.request, "urlretrieve", guard(urllib.request.urlretrieve))
    monkeypatch.setattr(urllib.request, "urlopen", guard(urllib.request.urlopen))


@pytest.fixture(scope="session", autouse=True)
def real_dev_files_untouched():
    before = _snapshot()
    yield
    assert _snapshot() == before, "a test touched real developer files (local config, .vscode or the venv .pth)"


@pytest.fixture(scope="session", autouse=True)
def _load_forge_app(tmp_path_factory):
    """Import pyforge.main once, from a scratch project that mounts the dummy plugin, before any
    test's tmp_project chdir runs — this is what mounts plugin commands (see pyforge/main.py)."""
    boot = tmp_path_factory.mktemp("boot")
    _install_project(boot)
    monkeypatch = pytest.MonkeyPatch()
    monkeypatch.chdir(boot)
    import pyforge.main  # noqa: F401

    monkeypatch.undo()


def _patchable_modules() -> list:
    for info in pkgutil.walk_packages(pyforge.__path__, prefix="pyforge."):
        if not info.name.startswith("pyforge.tests"):
            importlib.import_module(info.name)
    return [module for name, module in sys.modules.items() if name.split(".")[0] == "pyforge" and ".tests" not in name]


def patch_everywhere(monkeypatch, name: str, value) -> None:
    for module in _patchable_modules():
        if name in vars(module):
            monkeypatch.setattr(module, name, value)


@pytest.fixture
def linux_host(monkeypatch):
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    monkeypatch.setattr(platform, "machine", lambda: "x86_64")
    monkeypatch.setattr(os, "cpu_count", lambda: 8)


def _install_project(root: Path) -> None:
    (root / ".gitmodules").write_text(GITMODULES, encoding="utf-8")
    for vendor_dir in VENDOR_DIRS:
        (root / vendor_dir).mkdir(parents=True)
    (root / "forge.toml").write_text(FORGE_TOML, encoding="utf-8")
    shutil.copytree(FIXTURES / "dummy_plugin", root / "dummy_plugin", ignore=shutil.ignore_patterns("__pycache__"))
    build_dir = root / "build"
    build_dir.mkdir()
    for project in ("Oryx", "Tests"):
        (build_dir / f"{project}.make").write_text(make_file(project), encoding="utf-8")
    write_workspace(root)


@pytest.fixture
def tmp_project(tmp_path, monkeypatch, linux_host) -> Path:
    monkeypatch.chdir(tmp_path)
    # Isolates cache.user_cache_dir() from the real machine's ~/.cache (or platform
    # equivalent), so tests never read or write a developer's actual Premake cache.
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    _install_project(tmp_path)
    return tmp_path


@pytest.fixture
def forge(tmp_project):
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


def _git(cwd, *args):
    return subprocess.run(
        ["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "init.defaultBranch=main", *args],
        cwd=cwd, check=True, capture_output=True, text=True,
    ).stdout.strip()


@pytest.fixture
def remote(tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    work = tmp_path / "work"
    work.mkdir()
    _git(work, "init", "-q")
    (work / "a.h").write_text("1")
    _git(work, "add", ".")
    _git(work, "commit", "-qm", "one")
    _git(work, "tag", "v1")
    first = _git(work, "rev-parse", "HEAD")
    (work / "a.h").write_text("2")
    _git(work, "commit", "-qam", "two")
    bare = tmp_path / "remote.git"
    _git(tmp_path, "clone", "-q", "--bare", str(work), str(bare))
    return f"file://{bare}", first, _git(work, "rev-parse", "HEAD")


@pytest.fixture
def server(tmp_path):
    root = tmp_path / "www"
    root.mkdir()
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
    handler.log_message = lambda *args: None
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield root, f"http://127.0.0.1:{httpd.server_port}"
    httpd.shutdown()


def _tarball(path, members):
    with tarfile.open(path, "w:gz") as tar:
        for name, data in members.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))
