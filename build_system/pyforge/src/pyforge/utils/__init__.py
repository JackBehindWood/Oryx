"""General-purpose CLI utilities: platform info, subprocess execution,
filesystem and download helpers, and JSON read/write/merge — everything commands and the
editors/vscode generators share.
"""

from .download import download_file, download_with_progress, extract_archive, quiet_downloads, sha256_file
from .filesystem import ensure_directory, make_executable, remove_directory, remove_file
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
    "remove_file",
    "make_executable",
    "download_file",
    "download_with_progress",
    "extract_archive",
    "quiet_downloads",
    "sha256_file",
    "load_json",
    "write_json",
    "merge_by_key",
]
