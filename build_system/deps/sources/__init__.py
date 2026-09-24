"""Dependency sources: how a [dependencies] entry's files get onto disk, keyed by its `source`."""

from typing import Protocol

from ...config import DEPENDENCY_SOURCES
from ..resolve import ResolvedDependency


class Source(Protocol):
    def fetch(self, root, dep: ResolvedDependency) -> None: ...

    def update(self, root, dep: ResolvedDependency, rev: str | None) -> None: ...

    def remove(self, root, dep: ResolvedDependency) -> None: ...

    def pin(self, root, dep: ResolvedDependency) -> str: ...


SOURCES: dict[str, Source] = {}


def register(name: str, source: Source) -> None:
    # forge_dependency_sources() (wired in 4.2+) feeds plugin sources into this same register().
    SOURCES[name] = source
    DEPENDENCY_SOURCES.register(name)


def source_for(dep: ResolvedDependency) -> Source:
    return SOURCES[dep.spec.source]


def _register_builtins() -> None:
    from .local import LocalSource
    from .submodule import SubmoduleSource

    register("submodule", SubmoduleSource())
    register("local", LocalSource())


_register_builtins()
