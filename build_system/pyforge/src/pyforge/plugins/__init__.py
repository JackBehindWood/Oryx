"""pluggy hookspecs and loader for pyforge plugins (see hookspecs.py for the optional-pluggy design)."""

from .hookspecs import PROJECT_NAME, ForgeHookspecs, hookimpl, hookspec, pluggy
from .loader import PluginError, load_plugins
from .manager import get_plugin_manager

__all__ = ["PROJECT_NAME", "PluginError", "ForgeHookspecs", "hookspec", "hookimpl", "get_plugin_manager", "load_plugins", "pluggy"]
