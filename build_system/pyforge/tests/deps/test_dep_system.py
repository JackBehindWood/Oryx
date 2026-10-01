import os
import stat
import sys

import pytest

from conftest import make_run
from pyforge.config import Dependency
from pyforge.deps.resolve import DependencyError, ResolvedDependency, premake_config
from pyforge.deps.sources.system import SystemSource

SHIM = """#!/bin/sh
case "$1" in
  --exists) [ "$2" = fmt ] ;;
  --cflags) echo "-I/opt/fmt/include -DFMT" ;;
  --libs) echo "-L/opt/fmt/lib -lfmt" ;;
  --modversion) echo 10.2.1 ;;
esac
"""


@pytest.fixture
def pkg_config(tmp_path, monkeypatch):
    if sys.platform == "win32":
        pytest.skip("the pkg-config shim is a POSIX shell script")
    bin_dir = tmp_path / "shim"
    bin_dir.mkdir()
    script = bin_dir / "pkg-config"
    script.write_text(SHIM)
    script.chmod(script.stat().st_mode | stat.S_IEXEC)
    monkeypatch.setenv("PATH", f"{bin_dir}{os.pathsep}{os.environ['PATH']}")


def _dep(root, **spec):
    return ResolvedDependency("fmt", Dependency(source="system", **spec), None, root)


def test_found_via_pkg_config(pkg_config, tmp_path):
    dep = _dep(tmp_path, pkg_config="fmt")
    assert SystemSource().probe(tmp_path, dep) == (["-I/opt/fmt/include", "-DFMT"], ["-L/opt/fmt/lib", "-lfmt"])
    assert SystemSource().pin(tmp_path, dep) == "10.2.1"
    assert dep.present and dep.include is None and dep.sources is None


def test_not_found_names_the_fix(pkg_config, tmp_path):
    dep = _dep(tmp_path, pkg_config="nope")
    assert not dep.present
    with pytest.raises(DependencyError, match="not found via pkg-config"):
        SystemSource().fetch(tmp_path, dep)


def test_manual_paths(tmp_path, monkeypatch):
    monkeypatch.setenv("PATH", str(tmp_path / "empty"))
    (tmp_path / "inc").mkdir()
    (tmp_path / "libfoo.a").touch()
    dep = _dep(tmp_path, include="inc", lib="libfoo.a")
    cflags, libs = SystemSource().probe(tmp_path, dep)
    assert cflags == [f"-I{(tmp_path / 'inc').as_posix()}"] and libs == [(tmp_path / "libfoo.a").as_posix()]
    assert not _dep(tmp_path, include="missing").present


def test_premake_entry_and_cli_add(pkg_config, tmp_project, forge):
    result = forge("deps", "add", "fmt", "--system", "fmt")
    assert result.exit_code == 0, result.output
    assert 'pkg-config = "fmt"' in (tmp_project / "forge.toml").read_text()
    entry = premake_config(make_run(_project(tmp_project)))["dependencies"]["fmt"]
    assert entry == {"kind": "system", "cflags": ["-I/opt/fmt/include", "-DFMT"], "libs": ["-L/opt/fmt/lib", "-lfmt"], "defines": []}
    assert "(system)" in forge("deps", "status").output


def _project(root):
    from pyforge.project import Project

    return Project(root=root, config_file=root / "forge.toml")


def test_option_like_pkg_config_name_is_never_passed_to_pkg_config(monkeypatch):
    from pyforge.deps.sources import system

    monkeypatch.setattr(system.subprocess, "run", lambda *a, **k: pytest.fail("pkg-config must not run"))
    assert system._pkg_config("--exists", "--print-errors") is None
