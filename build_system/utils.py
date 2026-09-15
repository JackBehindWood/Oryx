import json
import platform
import shutil
import subprocess
from pathlib import Path


from .config import CONFIG_FILE

# ---------------------------------------------------------------------------
# Platform information
# ---------------------------------------------------------------------------

def get_os():
    """Return the current operating system."""
    return platform.system()


def get_architecture():
    """Return the current machine architecture."""
    return platform.machine()


# ---------------------------------------------------------------------------
# Command execution
# ---------------------------------------------------------------------------

def run_command(command, cwd=None, capture_output=True):
    """
    Run a command and return the completed subprocess result.

    Raises:
        subprocess.CalledProcessError: If the command fails.
        FileNotFoundError: If the executable cannot be found.
    """
    return subprocess.run(
        command,
        cwd=str(cwd) if cwd else None,
        capture_output=capture_output,
        text=True,
        check=True,
    )


# ---------------------------------------------------------------------------
# Filesystem helpers
# ---------------------------------------------------------------------------

def remove_directory(path):
    """Remove a directory if it exists."""
    path = Path(path)

    if path.exists():
        shutil.rmtree(path)


def ensure_directory(path):
    """Create a directory if it does not already exist."""
    Path(path).mkdir(parents=True, exist_ok=True)