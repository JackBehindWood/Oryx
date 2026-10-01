import os
import shutil
import stat
import sys
from pathlib import Path


def _retry_writable(function, path, _error):
    os.chmod(path, stat.S_IWRITE)
    function(path)


def rmtree_force(path, ignore_errors=False):
    """shutil.rmtree that also removes read-only files, which Windows refuses to delete (git objects)."""
    handler = {"onexc" if sys.version_info >= (3, 12) else "onerror": _retry_writable}
    try:
        shutil.rmtree(path, **handler)
    except OSError:
        if not ignore_errors:
            raise


def write_text_lf(path, text):
    """Write UTF-8 text with LF endings on every platform."""
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        file.write(text)


def remove_directory(path):
    """Remove a directory if it exists."""
    path = Path(path)

    if path.exists():
        rmtree_force(path)


def ensure_directory(path):
    """Create a directory if it does not already exist."""
    Path(path).mkdir(parents=True, exist_ok=True)


def remove_file(path):
    """Remove a file if it exists."""
    path = Path(path)

    if path.exists():
        path.unlink()


def make_executable(path):
    """Make a file executable on Unix-like systems."""
    path = Path(path)

    current_mode = path.stat().st_mode
    path.chmod(current_mode | 0o111)
