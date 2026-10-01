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
    import urllib.request

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
    import tarfile
    import zipfile

    archive = Path(archive)
    destination = Path(destination)

    destination.mkdir(parents=True, exist_ok=True)

    if archive.name.endswith((".tar.gz", ".tgz")):
        with tarfile.open(archive, "r:gz") as tar:
            tar.extractall(path=destination, filter="data")

    elif archive.suffix == ".zip":
        with zipfile.ZipFile(archive, "r") as zip_file:
            root = destination.resolve()
            for member in zip_file.namelist():
                if not (root / member).resolve().is_relative_to(root):
                    raise ValueError(f"Unsafe path in {archive.name}: {member}")
            zip_file.extractall(path=destination)

    else:
        raise ValueError(
            f"Unsupported archive format: {archive.name}"
        )


def download_with_progress(url, destination, description):
    from rich.console import Console
    from rich.progress import BarColumn, DownloadColumn, Progress, TimeRemainingColumn, TransferSpeedColumn

    with Progress(
        "[progress.description]{task.description}",
        BarColumn(),
        DownloadColumn(),
        TransferSpeedColumn(),
        TimeRemainingColumn(),
        console=Console(),
    ) as progress:
        task_id = progress.add_task(description, total=None)

        def reporthook(block_num, block_size, total_size):
            if total_size > 0:
                progress.update(task_id, total=total_size, completed=block_num * block_size)

        download_file(url, destination, reporthook=reporthook)


def sha256_file(path) -> str:
    import hashlib

    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for block in iter(lambda: file.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()
