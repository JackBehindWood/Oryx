"""General-purpose CLI utilities: platform info, subprocess execution,
filesystem helpers, and JSON read/write/merge — everything commands and the
editors/vscode generators share that isn't specific to fetching a toolchain (see
pyforge/setup/utils.py for that).
"""

from .filesystem import ensure_directory, remove_directory
from .json_files import load_json, merge_by_key, write_json
from .streaming import stream_command
from .system import child_env, get_architecture, get_macos_sdk_path, get_os, missing_module_hint, run_command

__all__ = [
    "get_os",
    "get_architecture",
    "get_macos_sdk_path",
    "run_command",
    "stream_command",
    "child_env",
    "missing_module_hint",
    "remove_directory",
    "ensure_directory",
    "load_json",
    "write_json",
    "merge_by_key",
]
