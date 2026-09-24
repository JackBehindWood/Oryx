from build_system import api
from build_system.config import ForgeConfig, RunContext, Suite
from build_system.plugins import PROJECT_NAME, ForgeHookspecs, get_plugin_manager, hookimpl, hookspec


def test_hookspecs_are_present():
    names = {name for name in dir(ForgeHookspecs) if not name.startswith("_")}
    assert names == {
        "forge_commands",
        "forge_config_schema",
        "forge_premake_args",
        "forge_pre_configure",
        "forge_post_compile",
        "forge_post_clean",
        "forge_test_env",
        "forge_editor_contributions",
        "forge_dependency_sources",
    }


def test_zero_hookimpl_call_returns_empty_list():
    pm = get_plugin_manager()
    assert pm.hook.forge_premake_args(ctx=None) == []
    assert pm.hook.forge_dependency_sources() == []


def test_marker_project_names():
    assert hookspec.project_name == PROJECT_NAME
    assert hookimpl.project_name == PROJECT_NAME


def test_api_reexports_are_identity_equal_to_originals():
    assert api.RunContext is RunContext
    assert api.ForgeConfig is ForgeConfig
    assert api.Suite is Suite
    assert api.API_VERSION == 1
