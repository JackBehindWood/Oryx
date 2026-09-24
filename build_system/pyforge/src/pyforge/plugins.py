"""pluggy hookspecs and loader for pyforge plugins.

pluggy is the `pyforge[plugins]` extra, not a core dependency (core install is typer + rich
only) — so importing it here must stay conditional. Without it, get_plugin_manager() returns a
_NullPluginManager whose every hook call is a no-op, which is correct and silent for the common
case (no `[plugins] paths` configured); only a project that actually sets `[plugins] paths`
without the extra installed sees an error, from load_plugins()."""

from __future__ import annotations

import importlib
import sys
from pathlib import Path

PROJECT_NAME = "pyforge"

try:
    import pluggy
except ImportError:
    pluggy = None


class PluginError(RuntimeError):
    pass


class _NullHooks:
    """Stands in for pluggy's `pm.hook`: every hook name is an always-empty-result call,
    matching a real PluginManager with zero registered implementations."""

    def __getattr__(self, _name):
        return lambda **_kwargs: []


class _NullPluginManager:
    """Stands in for pluggy.PluginManager when pluggy isn't installed. Valid only as long as
    no plugin is ever registered — load_plugins() only hands one out when `paths` is empty."""

    hook = _NullHooks()


if pluggy is not None:
    hookspec = pluggy.HookspecMarker(PROJECT_NAME)
    hookimpl = pluggy.HookimplMarker(PROJECT_NAME)

    class ForgeHookspecs:
        @hookspec
        def forge_commands(self, app) -> None: ...

        @hookspec
        def forge_config_schema(self): ...

        @hookspec
        def forge_premake_args(self, ctx) -> list[str] | None: ...

        @hookspec
        def forge_pre_configure(self, ctx) -> None: ...

        @hookspec
        def forge_post_compile(self, ctx) -> None: ...

        @hookspec
        def forge_post_clean(self, ctx) -> None: ...

        @hookspec
        def forge_test_env(self, ctx, suite) -> dict[str, str] | None: ...

        @hookspec
        def forge_editor_contributions(self, ctx) -> dict | None: ...

        @hookspec
        def forge_dependency_sources(self) -> dict[str, object] | None: ...
else:
    hookspec = None
    hookimpl = None
    ForgeHookspecs = None


def get_plugin_manager():
    """Checks `pluggy` (the module global) at call time rather than baking in a branch at
    import time, so tests can simulate "pluggy isn't installed" with a plain monkeypatch."""
    if pluggy is None:
        return _NullPluginManager()
    pm = pluggy.PluginManager(PROJECT_NAME)
    pm.add_hookspecs(ForgeHookspecs)
    return pm


def _import_plugin(root: Path, rel_path: str):
    """A `[plugins] paths` entry is a project-relative directory; import it as a dotted module,
    adding the project root to sys.path if it isn't already importable (e.g. a third-party plugin
    directory that isn't itself an installed package)."""
    name = rel_path.strip("/").replace("/", ".")
    try:
        return importlib.import_module(name)
    except ModuleNotFoundError:
        root_str = str(root)
        if root_str not in sys.path:
            sys.path.insert(0, root_str)
        try:
            return importlib.import_module(name)
        except ModuleNotFoundError as error:
            raise PluginError(f"[plugins] paths entry {rel_path!r} could not be imported as {name!r}: {error}") from error


def load_plugins(root: Path, paths: list[str]):
    """Load every `[plugins] paths` entry as a hookimpl-bearing module and register it."""
    if paths and pluggy is None:
        raise PluginError('[plugins] paths is set but pluggy is not installed; install it with `pip install "pyforge[plugins]"` (or `uv sync`).')
    pm = get_plugin_manager()
    for rel_path in paths:
        pm.register(_import_plugin(root, rel_path))
    return pm
