from pathlib import Path

import pytest

from pyforge import api, plugins
from pyforge.config import ForgeConfig, RunContext, Suite
from pyforge.plugins import PROJECT_NAME, ForgeHookspecs, PluginError, get_plugin_manager, hookimpl, hookspec


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


def test_without_pluggy_installed_the_plugin_manager_is_a_harmless_no_op(monkeypatch):
    """pluggy is the `pyforge[plugins]` extra, not a core dependency; a project with no
    [plugins] paths must work without it installed at all."""
    monkeypatch.setattr(plugins, "pluggy", None)
    pm = plugins.get_plugin_manager()
    assert pm.hook.forge_premake_args(ctx=None) == []
    assert pm.hook.forge_commands(app=None) == []


def test_without_pluggy_installed_configured_plugins_get_an_install_hint(monkeypatch, tmp_path):
    monkeypatch.setattr(plugins, "pluggy", None)
    with pytest.raises(PluginError, match='pip install "pyforge\\[plugins\\]"'):
        plugins.load_plugins(Path(tmp_path), ["some/plugin"])


def test_without_pluggy_installed_no_configured_plugins_is_fine(monkeypatch, tmp_path):
    monkeypatch.setattr(plugins, "pluggy", None)
    pm = plugins.load_plugins(Path(tmp_path), [])
    assert pm.hook.forge_test_env(ctx=None, suite=None) == []
