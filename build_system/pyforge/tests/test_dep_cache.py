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


def test_concurrent_record_pin_loses_no_updates(tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path))
    with ThreadPoolExecutor(8) as pool:
        list(pool.map(lambda i: cache.record_pin(tmp_path, f"dep{i}", "p"), range(32)))
    assert len(cache.read_pins()[str(tmp_path)]) == 32


def _record_many(cache_dir: str, worker: int) -> None:
    import os
    from pathlib import Path

    os.environ["PYFORGE_CACHE"] = cache_dir
    for index in range(25):
        cache.record_pin(Path(f"/proj{worker}"), f"dep{index}", "pin")


def test_record_pin_keeps_every_update_across_processes(tmp_path, monkeypatch):
    import multiprocessing

    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path))
    workers = [multiprocessing.Process(target=_record_many, args=(str(tmp_path), n)) for n in range(4)]
    for worker in workers:
        worker.start()
    for worker in workers:
        worker.join()
    pins = cache.read_pins()
    assert {root: len(named) for root, named in pins.items()} == {f"/proj{n}": 25 for n in range(4)}
