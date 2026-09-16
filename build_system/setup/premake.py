import platform
import subprocess

from rich.console import Console
from rich.progress import BarColumn, DownloadColumn, Progress, TimeRemainingColumn, TransferSpeedColumn

from ..config import PROJECT_ROOT
from ..utils import remove_directory
from .utils import (
    download_file,
    extract_archive,
    make_executable,
    remove_file,
)

console = Console()

# ---------------------------------------------------------------------------
# Premake configuration
# ---------------------------------------------------------------------------

# Only the downloaded binary + LICENSE.txt live under premake/bin/ (gitignored).
# premake/ itself also holds tracked, hand-written Lua helpers (common.lua,
# vendor.lua, included from the root premake5.lua) that are NOT touched here.
PREMAKE_DIR = PROJECT_ROOT / "premake" / "bin"


PREMAKE_VERSION = "5.0.0-beta8"

PREMAKE_URLS = {
    "Linux": (
        "https://github.com/premake/premake-core/releases/"
        "download/v5.0.0-beta8/"
        "premake-5.0.0-beta8-linux.tar.gz"
    ),
    "Darwin": (
        "https://github.com/premake/premake-core/releases/"
        "download/v5.0.0-beta8/"
        "premake-5.0.0-beta8-macosx.tar.gz"
    ),
    "Windows": (
        "https://github.com/premake/premake-core/releases/"
        "download/v5.0.0-beta8/"
        "premake-5.0.0-beta8-windows.zip"
    ),
}

PREMAKE_LICENSE_URL = (
    "https://raw.githubusercontent.com/premake/"
    "premake-core/master/LICENSE.txt"
)


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

def get_premake_executable():
    """Return the expected local Premake5 executable path."""
    if platform.system() == "Windows":
        return PREMAKE_DIR / "premake5.exe"

    return PREMAKE_DIR / "premake5"


# ---------------------------------------------------------------------------
# Detection
# ---------------------------------------------------------------------------

def check_local_premake():
    """Check whether the required local Premake5 executable exists."""
    executable = get_premake_executable()

    return executable.is_file()


# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

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


def install_premake():
    """Download and install the required Premake5 version locally."""
    system = platform.system()

    if system not in PREMAKE_URLS:
        console.print(f"[bold red]✗ Unsupported operating system:[/bold red] {system}")
        return False

    url = PREMAKE_URLS[system]
    filename = url.split("/")[-1]
    archive_path = PREMAKE_DIR / filename

    console.print(f"[bold blue]📥 Downloading Premake5 v{PREMAKE_VERSION}...[/bold blue]")

    PREMAKE_DIR.mkdir(parents=True, exist_ok=True)

    try:
        # Download Premake
        _download_with_progress(url, archive_path, filename)
        console.print(f"[green]✓ Downloaded {filename}[/green]\n")

        # Extract Premake
        console.print(f"[bold blue]📦 Extracting {filename}...[/bold blue]")
        extract_archive(archive_path, PREMAKE_DIR)
        console.print("[green]✓ Extracted successfully[/green]\n")

        # Remove downloaded archive
        remove_file(archive_path)

        # Download Premake licence
        console.print("[bold blue]📄 Downloading Premake5 licence...[/bold blue]")
        license_path = PREMAKE_DIR / "LICENSE.txt"

        try:
            download_file(PREMAKE_LICENSE_URL, license_path)
            console.print(f"[green]✓ Saved licence to {license_path}[/green]\n")
        except Exception as error:
            console.print(f"[yellow]⚠️ Could not download licence: {error}[/yellow]\n")

        # Make executable on Unix-like systems
        executable = get_premake_executable()
        if system in {"Linux", "Darwin"} and executable.exists():
            make_executable(executable)
            console.print(f"[green]✓ Made executable:[/green] {executable}\n")

        # -------------------------------------------------------------------
        # Clean up extraneous files (keep ONLY the executable & LICENSE.txt)
        # -------------------------------------------------------------------
        allowed_files = {executable.name.lower(), "license.txt"}

        for item in PREMAKE_DIR.iterdir():
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


def installed_version(executable=None) -> str | None:
    """Report the version string the local Premake5 binary identifies itself as."""
    executable = executable or get_premake_executable()
    if not executable.is_file():
        return None
    try:
        result = subprocess.run([str(executable), "--version"], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def update_premake():
    """Force a re-download of the pinned Premake5 version, overwriting whatever
    is currently installed — unlike ensure_premake(), which leaves an existing
    install alone even if it doesn't match PREMAKE_VERSION."""
    previous = installed_version()
    if previous:
        console.print(f"[dim]Currently installed: {previous}[/dim]")

    console.print(f"[bold blue]📦 Updating Premake5 to v{PREMAKE_VERSION}...[/bold blue]\n")

    if not install_premake():
        console.print("[bold red]✗ Could not update Premake5.[/bold red]")
        return None

    executable = get_premake_executable()
    if not check_local_premake():
        console.print("[bold red]✗ Update reported success but the executable is missing.[/bold red]")
        return None

    new_version = installed_version(executable)
    console.print(f"[green]✓ premake5: now {new_version or f'v{PREMAKE_VERSION}'} at {executable}[/green]\n")
    return executable


# ---------------------------------------------------------------------------
# Public setup function
# ---------------------------------------------------------------------------

def ensure_premake():
    """
    Ensure the required local Premake5 installation exists.

    Premake is always provided locally by the project. A system-wide
    Premake installation is never used.

    Returns:
        Path to the local Premake5 executable.
        None if Premake could not be installed.
    """
    executable = get_premake_executable()

    if check_local_premake():
        console.print(f"[green]✓ premake5:[/green] found locally at {executable}\n")
        return executable

    console.print("[yellow]✗ premake5: local installation not found.[/yellow]")
    console.print(f"  [dim]Expected: {executable}[/dim]\n")

    console.print("[bold blue]📦 Installing Premake5 locally...[/bold blue]\n")

    if install_premake():
        executable = get_premake_executable()

        if check_local_premake():
            console.print(f"[green]✓ premake5: installed locally at {executable}[/green]\n")
            return executable

    console.print("[bold red]✗ Could not install the required local Premake5.[/bold red]")
    return None
