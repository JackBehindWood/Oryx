import hashlib
from dataclasses import dataclass
from typing import Callable

import pytest

from conftest import _tarball
from pyforge import cache
from pyforge.config import Dependency
from pyforge.deps.resolve import DependencyError, ResolvedDependency
from pyforge.deps.sources import SOURCES


@dataclass
class Case:
    source: str
    add_args: list[str]
    recorded: str
    corrupt: Callable[[], ResolvedDependency]
    mismatch: str


def _archive(server) -> Case:
    root, base = server
    _tarball(root / "lib.tar.gz", {"a.h": b"x"})
    sha = hashlib.sha256((root / "lib.tar.gz").read_bytes()).hexdigest()
    spec = Dependency(source="archive", url=f"{base}/lib.tar.gz", sha256="0" * 64)
    return Case("archive", ["--archive", f"{base}/lib.tar.gz"], sha, lambda: ResolvedDependency("lib", spec, cache.deps_dir("lib", "0" * 12)), "sha256 mismatch")


def _file(server) -> Case:
    root, base = server
    (root / "stb.h").write_bytes(b"// stb")
    sha = hashlib.sha256(b"// stb").hexdigest()
    spec = Dependency(source="file", url=f"{base}/stb.h", sha256="0" * 64)
    return Case("file", ["--file", f"{base}/stb.h"], sha, lambda: ResolvedDependency("lib", spec, cache.deps_dir("lib", "0" * 12)), "sha256 mismatch")


def _git(remote) -> Case:
    url, first, _ = remote
    spec = Dependency(source="git", url=url, rev="main", commit="0" * 40)
    return Case("git", ["--git", url, "--rev", "v1"], first, lambda: ResolvedDependency("lib", spec, cache.deps_dir("lib", "0" * 40)), "pins commit|Dependency|clone|fatal")


@pytest.fixture(params=["archive", "file", "git"])
def case(request, server, remote) -> Case:
    return {"archive": lambda: _archive(server), "file": lambda: _file(server), "git": lambda: _git(remote)}[request.param]()


def test_cli_add_records_the_source_and_its_pin(case, tmp_project, forge):
    result = forge("deps", "add", "lib", *case.add_args)
    assert result.exit_code == 0, result.output
    text = (tmp_project / "forge.toml").read_text()
    assert f'source = "{case.source}"' in text and case.recorded in text


def test_failed_integrity_check_leaves_no_cache_entry(case, tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    dep = case.corrupt()
    with pytest.raises(DependencyError, match=case.mismatch):
        SOURCES[case.source].fetch(tmp_path, dep)
    assert not dep.dir.exists()
