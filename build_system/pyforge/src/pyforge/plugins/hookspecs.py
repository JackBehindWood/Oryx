"""pluggy hookspecs for pyforge plugins.

pluggy is the `pyforge[plugins]` extra, not a core dependency (core install is typer + rich
only) — so importing it here must stay conditional. Without it, get_plugin_manager() returns a
_NullPluginManager whose every hook call is a no-op, which is correct and silent for the common
case (no `[plugins] paths` configured); only a project that actually sets `[plugins] paths`
without the extra installed sees an error, from load_plugins()."""

from __future__ import annotations

PROJECT_NAME = "pyforge"

try:
    import pluggy
except ImportError:
    pluggy = None

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
