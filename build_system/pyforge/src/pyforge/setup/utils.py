import os
import tarfile
import urllib.request
import zipfile
from pathlib import Path


def download_file(url, destination, reporthook=None):
    """
    Download a file from a URL.

    Args:
        url: URL to download.
        destination: Destination path.
        reporthook: Optional urlretrieve-style progress callback
            (block_num, block_size, total_size).
    """
    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)

    urllib.request.urlretrieve(url, destination, reporthook=reporthook)


def extract_archive(archive, destination):
    """
    Extract a .tar.gz or .zip archive.

    Args:
        archive: Archive path.
        destination: Extraction directory.

    Raises:
        ValueError: If the archive format is unsupported.
    """
    archive = Path(archive)
    destination = Path(destination)

    destination.mkdir(parents=True, exist_ok=True)

    if archive.name.endswith(".tar.gz"):
        with tarfile.open(archive, "r:gz") as tar:
            tar.extractall(path=destination)

    elif archive.suffix == ".zip":
        with zipfile.ZipFile(archive, "r") as zip_file:
            zip_file.extractall(path=destination)

    else:
        raise ValueError(
            f"Unsupported archive format: {archive.name}"
        )


def make_executable(path):
    """Make a file executable on Unix-like systems."""
    path = Path(path)

    current_mode = path.stat().st_mode
    path.chmod(current_mode | 0o111)

def remove_file(path):
    """Remove a file if it exists."""
    path = Path(path)

    if path.exists():
        path.unlink()
