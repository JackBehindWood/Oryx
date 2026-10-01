from concurrent.futures import ThreadPoolExecutor

from pyforge import cache
from conftest import make_run
from pyforge.config import Dependency
from pyforge.deps.resolve import dependency_dir


def test_atomic_extract_concurrent_writers_leave_one_entry(tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path))
    dest = cache.deps_dir("lib", "abc")

    def extract(tmp):
        tmp.mkdir()
        (tmp / "f.txt").write_text("x")

    with ThreadPoolExecutor(8) as pool:
        results = list(pool.map(lambda _: cache.atomic_extract(dest, extract), range(8)))
    assert results.count(True) == 1
    assert (dest / "f.txt").read_text() == "x"
    assert [p.name for p in dest.parent.iterdir()] == ["abc"]


def test_dependency_dir_git_and_archive_resolve_into_cache(project, tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path))
    cfg = make_run(project).config
    git = Dependency(source="git", url="u", commit="deadbeef")
    assert dependency_dir(project, cfg, "lib", git) == cache.deps_dir("lib", "deadbeef")
    assert dependency_dir(project, cfg, "lib", Dependency(source="local")) == project.path(cfg.build.dependencies_dir) / "lib"


def test_clean_cache_unused_removes_only_orphan(project, tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    for pin in ("keep", "orphan"):
        cache.deps_dir("lib", pin).mkdir(parents=True)
    cache.record_pin(project.root, "lib", "keep")
    run = make_run(project)
    from pyforge.commands.deps import clean_cache

    class Ctx:
        obj = run

    clean_cache(Ctx(), unused=True, yes=True)
    assert cache.deps_dir("lib", "keep").is_dir()
    assert not cache.deps_dir("lib", "orphan").exists()
    clean_cache(Ctx(), unused=False, yes=True)
    assert not cache.deps_root().exists()


def test_unused_entries_keeps_only_referenced(tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    project = tmp_path / "proj"
    project.mkdir()
    for pin in ("keep", "orphan"):
        cache.deps_dir("lib", pin).mkdir(parents=True)
    cache.record_pin(project, "lib", "keep")
    assert [p.name for p in cache.unused_entries()] == ["orphan"]
