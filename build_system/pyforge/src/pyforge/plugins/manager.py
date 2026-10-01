from __future__ import annotations

from .hookspecs import PROJECT_NAME


class _NullHooks:
    """Stands in for pluggy's `pm.hook`: every hook name is an always-empty-result call,
    matching a real PluginManager with zero registered implementations."""

    def __getattr__(self, _name):
        return lambda **_kwargs: []


class _NullPluginManager:
    """Stands in for pluggy.PluginManager when pluggy isn't installed. Valid only as long as
    no plugin is ever registered — load_plugins() only hands one out when `paths` is empty."""

    hook = _NullHooks()


def get_plugin_manager():
    """Reads `pluggy` off the package at call time, so tests can simulate "pluggy isn't
    installed" with a plain monkeypatch of `pyforge.plugins.pluggy`."""
    from .. import plugins

    if plugins.pluggy is None:
        return _NullPluginManager()
    pm = plugins.pluggy.PluginManager(PROJECT_NAME)
    pm.add_hookspecs(plugins.ForgeHookspecs)
    return pm
