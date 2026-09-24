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
