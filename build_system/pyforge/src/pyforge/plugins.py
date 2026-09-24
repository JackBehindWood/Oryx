"""pluggy hookspecs and loader for pyforge plugins."""

import importlib
import sys
from pathlib import Path

import pluggy

PROJECT_NAME = "pyforge"
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


def get_plugin_manager() -> pluggy.PluginManager:
    pm = pluggy.PluginManager(PROJECT_NAME)
    pm.add_hookspecs(ForgeHookspecs)
    return pm


class PluginError(RuntimeError):
    pass


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


def load_plugins(root: Path, paths: list[str]) -> pluggy.PluginManager:
    """Load every `[plugins] paths` entry as a hookimpl-bearing module and register it."""
    pm = get_plugin_manager()
    for rel_path in paths:
        pm.register(_import_plugin(root, rel_path))
    return pm
