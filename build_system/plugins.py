"""pluggy hookspecs for pyforge plugins. No loader yet — nothing calls pm.hook.* until 4.2."""

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
