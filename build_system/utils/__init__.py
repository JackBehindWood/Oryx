"""General-purpose CLI utilities: platform info, subprocess execution,
filesystem helpers, and JSON read/write/merge — everything commands and the
vscode/* generators share that isn't specific to fetching a toolchain (see
build_system/setup/utils.py for that).
"""

from .filesystem import ensure_directory, remove_directory
from .json_files import load_json, merge_by_key, write_json
from .system import get_architecture, get_macos_sdk_path, get_os, run_command

__all__ = [
    "get_os",
    "get_architecture",
    "get_macos_sdk_path",
    "run_command",
    "remove_directory",
    "ensure_directory",
    "load_json",
    "write_json",
    "merge_by_key",
]
