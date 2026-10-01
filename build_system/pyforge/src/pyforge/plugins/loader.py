from __future__ import annotations

import importlib
import sys
from pathlib import Path

from .manager import get_plugin_manager


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


def load_plugins(root: Path, paths: list[str]):
    """Load every `[plugins] paths` entry as a hookimpl-bearing module and register it."""
    from .. import plugins

    if paths and plugins.pluggy is None:
        raise PluginError('[plugins] paths is set but pluggy is not installed; install it with `pip install "pyforge[plugins]"` (or `uv sync`).')
    pm = get_plugin_manager()
    for rel_path in paths:
        pm.register(_import_plugin(root, rel_path))
    return pm
