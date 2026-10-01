import platform
import shutil
import subprocess
from pathlib import Path

from rich.console import Console
from rich.markup import escape

from ..cache import premake_dir
from ..project import Project
from ..utils import download_file, download_with_progress, extract_archive, make_executable, remove_directory, remove_file
from .assets import DEFAULT_PREMAKE_VERSION, PREMAKE_LICENSE_URL, asset_name, premake_url
from .checksum import ChecksumError, latest_release_version, verify_checksum  # noqa: F401

console = Console()

SYSTEM_PREMAKE_HINT = "  [dim]forge premake install can't fetch a build for this platform; install premake5 yourself and either put it on PATH or set \\[premake] path in forge.toml.[/dim]"


def get_premake_executable(bin_dir: Path) -> Path:
    """Return the expected local Premake5 executable path."""
    return bin_dir / ("premake5.exe" if platform.system() == "Windows" else "premake5")


def resolve_bin_dir(project: Project, path_override: str, version: str) -> Path:
    """[premake] path when set, otherwise the shared user cache keyed by version — never
    a project-local directory, so worktrees and other projects on the same machine share
    one download per version."""
    if path_override:
        return project.path(path_override)
    return premake_dir(version)


def check_local_premake(bin_dir: Path) -> bool:
    """Check whether the required local Premake5 executable exists."""
    return get_premake_executable(bin_dir).is_file()


def _offline_refusal(bin_dir: Path, version: str) -> None:
    console.print(f"[bold red]✗ Missing: premake5 v{version} (expected at {bin_dir}); --offline forbids downloading it.[/bold red]")
    console.print("  [dim]Run once without --offline: forge premake install[/dim]")


def install_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION, offline: bool = False):
    """Download, checksum-verify, and install the given Premake5 version locally."""
    if offline:
        _offline_refusal(bin_dir, version)
        return False
    system = platform.system()
    url = premake_url(version, system, platform.machine())

    if url is None:
        console.print(f"[bold red]✗ No Premake5 build for {system}/{platform.machine()}.[/bold red]")
        console.print(SYSTEM_PREMAKE_HINT)
        return False

    filename = url.split("/")[-1]
    archive_path = bin_dir / filename

    console.print(f"[bold blue]📥 Downloading Premake5 v{version}...[/bold blue]")

    bin_dir.mkdir(parents=True, exist_ok=True)

    try:
        download_with_progress(url, archive_path, filename)
        console.print(f"[green]✓ Downloaded {filename}[/green]\n")

        verify_checksum(archive_path, version, filename)
        console.print("[green]✓ Checksum verified[/green]\n")

        console.print(f"[bold blue]📦 Extracting {filename}...[/bold blue]")
        extract_archive(archive_path, bin_dir)
        console.print("[green]✓ Extracted successfully[/green]\n")

        remove_file(archive_path)

        console.print("[bold blue]📄 Downloading Premake5 licence...[/bold blue]")
        license_path = bin_dir / "LICENSE.txt"

        try:
            download_file(PREMAKE_LICENSE_URL.format(version=version), license_path)
            console.print(f"[green]✓ Saved licence to {license_path}[/green]\n")
        except Exception as error:
            console.print(f"[yellow]⚠️ Could not download licence: {escape(str(error))}[/yellow]\n")

        executable = get_premake_executable(bin_dir)
        if system in {"Linux", "Darwin"} and executable.exists():
            make_executable(executable)
            console.print(f"[green]✓ Made executable:[/green] {executable}\n")

        # Keep only the executable & LICENSE.txt; discard whatever else the archive unpacked.
        allowed_files = {executable.name.lower(), "license.txt"}

        for item in bin_dir.iterdir():
            if item.is_file() and item.name.lower() not in allowed_files:
                remove_file(item)
            elif item.is_dir():
                remove_directory(item)

        return True

    except ChecksumError as error:
        console.print(f"[bold red]✗ Checksum mismatch, refusing to install:[/bold red] {escape(str(error))}")
        remove_file(archive_path)
        return False
    except Exception as error:
        console.print(f"[bold red]✗ Failed to download/extract Premake5:[/bold red] {escape(str(error))}")
        remove_file(archive_path)
        return False


def system_premake(path_override: str = "") -> Path | None:
    """premake5 from PATH, for a platform with no release asset (Linux arm64) and no [premake] path."""
    if path_override or asset_name(platform.system(), platform.machine()) is not None:
        return None
    found = shutil.which("premake5")
    return Path(found) if found else None


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


def update_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION, offline: bool = False):
    """Force a re-download of `version`, overwriting whatever is currently installed at
    `bin_dir` — unlike ensure_premake(), which leaves an existing install alone."""
    executable = get_premake_executable(bin_dir)
    previous = installed_version(executable)
    if previous:
        console.print(f"[dim]Currently installed: {previous}[/dim]")

    console.print(f"[bold blue]📦 Updating Premake5 to v{version}...[/bold blue]\n")

    if not install_premake(bin_dir, version, offline):
        console.print("[bold red]✗ Could not update Premake5.[/bold red]")
        return None

    if not check_local_premake(bin_dir):
        console.print("[bold red]✗ Update reported success but the executable is missing.[/bold red]")
        return None

    new_version = installed_version(executable)
    console.print(f"[green]✓ premake5: now {new_version or f'v{version}'} at {executable}[/green]\n")
    return executable


def ensure_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION, path_override: str = "", offline: bool = False):
    """
    Ensure the required local Premake5 installation exists.

    Premake is provided locally (the shared user cache by default). A system-wide
    premake5 is used only where no release asset exists for this platform (Linux arm64)
    and [premake] path isn't set.

    Returns:
        Path to the local Premake5 executable.
        None if Premake could not be installed.
    """
    executable = get_premake_executable(bin_dir)

    if check_local_premake(bin_dir):
        console.print(f"[green]✓ premake5:[/green] found locally at {executable}\n")
        return executable

    if not path_override and asset_name(platform.system(), platform.machine()) is None:
        system = system_premake()
        if system is None:
            console.print(f"[bold red]✗ No Premake5 build for {platform.system()}/{platform.machine()}, and no premake5 on PATH.[/bold red]")
            console.print(SYSTEM_PREMAKE_HINT)
            return None
        console.print(f"[green]✓ premake5:[/green] using system premake5 at {system} ({installed_version(system) or 'unknown version'})\n")
        return system

    if offline:
        _offline_refusal(bin_dir, version)
        return None

    console.print("[yellow]✗ premake5: local installation not found.[/yellow]")
    console.print(f"  [dim]Expected: {executable}[/dim]\n")

    console.print("[bold blue]📦 Installing Premake5 locally...[/bold blue]\n")

    if install_premake(bin_dir, version):
        if check_local_premake(bin_dir):
            console.print(f"[green]✓ premake5: installed locally at {executable}[/green]\n")
            return executable

    console.print("[bold red]✗ Could not install the required local Premake5.[/bold red]")
    return None
