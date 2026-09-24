import platform
import subprocess
from pathlib import Path

from rich.console import Console
from rich.progress import BarColumn, DownloadColumn, Progress, TimeRemainingColumn, TransferSpeedColumn

from ..utils import remove_directory
from .utils import (
    download_file,
    extract_archive,
    make_executable,
    remove_file,
)

console = Console()

DEFAULT_PREMAKE_VERSION = "5.0.0-beta8"

PREMAKE_ASSETS = {
    "Linux": "linux.tar.gz",
    "Darwin": "macosx.tar.gz",
    "Windows": "windows.zip",
}

PREMAKE_LICENSE_URL = (
    "https://raw.githubusercontent.com/premake/"
    "premake-core/master/LICENSE.txt"
)


def premake_url(version: str, system: str) -> str:
    return (
        "https://github.com/premake/premake-core/releases/"
        f"download/v{version}/premake-{version}-{PREMAKE_ASSETS[system]}"
    )


def get_premake_executable(bin_dir: Path) -> Path:
    """Return the expected local Premake5 executable path."""
    return bin_dir / ("premake5.exe" if platform.system() == "Windows" else "premake5")


def check_local_premake(bin_dir: Path) -> bool:
    """Check whether the required local Premake5 executable exists."""
    return get_premake_executable(bin_dir).is_file()


def _download_with_progress(url: str, destination, description: str):
    with Progress(
        "[progress.description]{task.description}",
        BarColumn(),
        DownloadColumn(),
        TransferSpeedColumn(),
        TimeRemainingColumn(),
        console=console,
    ) as progress:
        task_id = progress.add_task(description, total=None)

        def reporthook(block_num, block_size, total_size):
            if total_size > 0:
                progress.update(task_id, total=total_size, completed=block_num * block_size)

        download_file(url, destination, reporthook=reporthook)


def install_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION):
    """Download and install the required Premake5 version locally."""
    system = platform.system()

    if system not in PREMAKE_ASSETS:
        console.print(f"[bold red]✗ Unsupported operating system:[/bold red] {system}")
        return False

    url = premake_url(version, system)
    filename = url.split("/")[-1]
    archive_path = bin_dir / filename

    console.print(f"[bold blue]📥 Downloading Premake5 v{version}...[/bold blue]")

    bin_dir.mkdir(parents=True, exist_ok=True)

    try:
        # Download Premake
        _download_with_progress(url, archive_path, filename)
        console.print(f"[green]✓ Downloaded {filename}[/green]\n")

        # Extract Premake
        console.print(f"[bold blue]📦 Extracting {filename}...[/bold blue]")
        extract_archive(archive_path, bin_dir)
        console.print("[green]✓ Extracted successfully[/green]\n")

        # Remove downloaded archive
        remove_file(archive_path)

        # Download Premake licence
        console.print("[bold blue]📄 Downloading Premake5 licence...[/bold blue]")
        license_path = bin_dir / "LICENSE.txt"

        try:
            download_file(PREMAKE_LICENSE_URL, license_path)
            console.print(f"[green]✓ Saved licence to {license_path}[/green]\n")
        except Exception as error:
            console.print(f"[yellow]⚠️ Could not download licence: {error}[/yellow]\n")

        # Make executable on Unix-like systems
        executable = get_premake_executable(bin_dir)
        if system in {"Linux", "Darwin"} and executable.exists():
            make_executable(executable)
            console.print(f"[green]✓ Made executable:[/green] {executable}\n")

        # -------------------------------------------------------------------
        # Clean up extraneous files (keep ONLY the executable & LICENSE.txt)
        # -------------------------------------------------------------------
        allowed_files = {executable.name.lower(), "license.txt"}

        for item in bin_dir.iterdir():
            if item.is_file() and item.name.lower() not in allowed_files:
                remove_file(item)
            elif item.is_dir():
                # Remove extra directories if any were unpacked
                remove_directory(item)

        return True

    except Exception as error:
        console.print(f"[bold red]✗ Failed to download/extract Premake5:[/bold red] {error}")
        remove_file(archive_path)
        return False


def installed_version(executable: Path) -> str | None:
    """Report the version string the local Premake5 binary identifies itself as."""
    if not executable.is_file():
        return None
    try:
        # Outside the repo root, or premake loads premake5.lua and fails on missing --python-* args.
        result = subprocess.run([str(executable), "--version"], cwd=executable.parent, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def update_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION):
    """Force a re-download of the pinned Premake5 version, overwriting whatever
    is currently installed — unlike ensure_premake(), which leaves an existing
    install alone even if it doesn't match the pinned version."""
    executable = get_premake_executable(bin_dir)
    previous = installed_version(executable)
    if previous:
        console.print(f"[dim]Currently installed: {previous}[/dim]")

    console.print(f"[bold blue]📦 Updating Premake5 to v{version}...[/bold blue]\n")

    if not install_premake(bin_dir, version):
        console.print("[bold red]✗ Could not update Premake5.[/bold red]")
        return None

    if not check_local_premake(bin_dir):
        console.print("[bold red]✗ Update reported success but the executable is missing.[/bold red]")
        return None

    new_version = installed_version(executable)
    console.print(f"[green]✓ premake5: now {new_version or f'v{version}'} at {executable}[/green]\n")
    return executable


# ---------------------------------------------------------------------------
# Public setup function
# ---------------------------------------------------------------------------

def ensure_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION):
    """
    Ensure the required local Premake5 installation exists.

    Premake is always provided locally by the project. A system-wide
    Premake installation is never used.

    Returns:
        Path to the local Premake5 executable.
        None if Premake could not be installed.
    """
    executable = get_premake_executable(bin_dir)

    if check_local_premake(bin_dir):
        console.print(f"[green]✓ premake5:[/green] found locally at {executable}\n")
        return executable

    console.print("[yellow]✗ premake5: local installation not found.[/yellow]")
    console.print(f"  [dim]Expected: {executable}[/dim]\n")

    console.print("[bold blue]📦 Installing Premake5 locally...[/bold blue]\n")

    if install_premake(bin_dir, version):
        if check_local_premake(bin_dir):
            console.print(f"[green]✓ premake5: installed locally at {executable}[/green]\n")
            return executable

    console.print("[bold red]✗ Could not install the required local Premake5.[/bold red]")
    return None
