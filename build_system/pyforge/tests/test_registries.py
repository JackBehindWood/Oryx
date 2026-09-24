"""Register a throwaway 'stub' entry per registry, round-trip, and restore state.

Mirrors test_registry.py's snapshot/restore pattern for the 4.1 registries:
generators, runners, editors, and (as a regression check) the pre-existing
deps/sources registry.
"""

from pyforge.config import DEPENDENCY_SOURCES, EDITORS, GENERATORS
from pyforge.deps.sources import SOURCES
from pyforge.editors import registry as editor_registry
from pyforge.runners import RUNNERS
from pyforge.setup import generators as generators_registry


def test_generators_registry_round_trips(monkeypatch):
    monkeypatch.setattr(GENERATORS, "_names", list(GENERATORS._names))
    monkeypatch.setattr(generators_registry, "COMPILE_COMMAND_BUILDERS", dict(generators_registry.COMPILE_COMMAND_BUILDERS))

    generators_registry.register("stub", lambda makefile_dir, token, jobs: ["stub"])

    assert "stub" in GENERATORS
    assert generators_registry.COMPILE_COMMAND_BUILDERS["stub"](None, "token", 0) == ["stub"]


def test_runners_registry_has_builtins():
    assert set(RUNNERS) == {"doctest", "pytest"}


def test_editors_registry_round_trips(monkeypatch):
    monkeypatch.setattr(EDITORS, "_names", list(EDITORS._names))
    monkeypatch.setattr("pyforge.editors.registry.EDITOR_COMMANDS", dict(editor_registry.EDITOR_COMMANDS))

    editor_registry.register("stub", lambda ctx, remember: None)

    assert "stub" in EDITORS
    assert editor_registry.editor_command("stub") is not None


def test_deps_sources_registry_still_has_builtins():
    assert "submodule" in SOURCES
    assert "local" in SOURCES
    assert "submodule" in DEPENDENCY_SOURCES
    assert "local" in DEPENDENCY_SOURCES
