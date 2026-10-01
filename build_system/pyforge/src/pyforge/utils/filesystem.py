import shutil
from pathlib import Path


def remove_directory(path):
    """Remove a directory if it exists."""
    path = Path(path)

    if path.exists():
        shutil.rmtree(path)


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
