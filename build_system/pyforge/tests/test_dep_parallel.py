import time

import pytest

from conftest import make_run
from pyforge.config import Dependency
from pyforge.deps import resolve
from pyforge.deps.resolve import DependencyError, ResolvedDependency
from pyforge.deps.sources import SOURCES


class Slow:
    def __init__(self, fail=()):
        self.fail = fail

    def fetch(self, root, dep):
        time.sleep(0.3)
        if dep.name in self.fail:
            raise DependencyError(f"{dep.name} broke")


def _deps(tmp_path, *names):
    return [ResolvedDependency(n, Dependency(source="git", url="u", commit="c"), tmp_path / n) for n in names]


def test_network_fetches_run_concurrently(project, tmp_path, monkeypatch):
    monkeypatch.setitem(SOURCES, "git", Slow())
    start = time.perf_counter()
    outcomes = resolve.fetch_each(make_run(project), _deps(tmp_path, "c", "a", "b"))
    assert time.perf_counter() - start < 0.8
    assert [(dep.name, error) for dep, error in outcomes] == [("a", None), ("b", None), ("c", None)]


def test_one_failure_still_reports_every_outcome(project, tmp_path, monkeypatch):
    monkeypatch.setitem(SOURCES, "git", Slow(fail=("b",)))
    outcomes = resolve.fetch_each(make_run(project), _deps(tmp_path, "a", "b", "c"))
    assert [error is None for _, error in outcomes] == [True, False, True]
    with pytest.raises(DependencyError, match="b broke"):
        resolve.fetch(make_run(project), _deps(tmp_path, "a", "b", "c"))
