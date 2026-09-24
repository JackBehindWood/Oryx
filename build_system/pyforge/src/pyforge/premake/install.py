import platform
import subprocess
from pathlib import Path

from rich.console import Console
from rich.markup import escape

from ..cache import premake_dir
from ..project import Project
from ..setup.utils import download_file, extract_archive, make_executable, remove_file
from ..utils import remove_directory

console = Console()

DEFAULT_PREMAKE_VERSION = "5.0.0-beta8"

RELEASES_API = "https://api.github.com/repos/premake/premake-core/releases"

PREMAKE_LICENSE_URL = "https://raw.githubusercontent.com/premake/premake-core/v{version}/LICENSE.txt"

_ARM_MACHINES = {"arm64", "aarch64"}

# The hashes pyforge ships for its own pinned default version, verified independently of
# GitHub's API at release time — a defense a compromised/spoofed API response can't get past
# for a version this table covers. Any other version relies on GitHub's asset `digest` field
# alone (still TLS-fetched from api.github.com, not the download host).
KNOWN_HASHES = {
    "5.0.0-beta8": {
        "premake-5.0.0-beta8-linux.tar.gz": "63edd3e7461eebdd45b500a3c7e8ad4e7a67d68f230010f9a97cbb71b4ec59c8",
        "premake-5.0.0-beta8-macosx.tar.gz": "fa73a46f093fa6f17494a3d063421aa6cae3ea825a61c62dd59fc2f07a256d03",
        "premake-5.0.0-beta8-macosx-x64.tar.gz": "84b5fa5a432dcebdc3dd12e8677d10e38e5b32a3fe06d83ae68967e4f5e2db8a",
        "premake-5.0.0-beta8-windows.zip": "e64ce2ed8778e0098f63674cca61fe33941b5f0c8d9a4afd651152bdea3758ab",
    },
}


class ChecksumError(RuntimeError):
    pass


def asset_name(system: str, machine: str) -> str | None:
    """The Premake release asset for this OS/architecture, or None when there isn't one
    (Linux has no arm64 build yet: use [premake] path to point at your own)."""
    machine = machine.lower()
    if system == "Linux":
        return "linux.tar.gz"
    if system == "Darwin":
        return "macosx.tar.gz" if machine in _ARM_MACHINES else "macosx-x64.tar.gz"
    if system == "Windows":
        return "windows.zip"
    return None


def premake_url(version: str, system: str, machine: str) -> str | None:
    suffix = asset_name(system, machine)
    if suffix is None:
        return None
    return f"https://github.com/premake/premake-core/releases/download/v{version}/premake-{version}-{suffix}"


def get_premake_executable(bin_dir: Path) -> Path:
    """Return the expected local Premake5 executable path."""
    return bin_dir / ("premake5.exe" if platform.system() == "Windows" else "premake5")


def lua_scripts_dir() -> Path:
    """Directory pyforge's own Premake helpers (lua/forge.lua) ship in. Passed to Premake via
    --scripts so a project's premake5.lua can `require "forge"` regardless of where pyforge is
    installed, instead of hardcoding a path to it."""
    return Path(__file__).resolve().parent.parent / "lua"


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


def _download_with_progress(url: str, destination, description: str):
    from rich.progress import BarColumn, DownloadColumn, Progress, TimeRemainingColumn, TransferSpeedColumn

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


def _sha256(path: Path) -> str:
    import hashlib

    digest = hashlib.sha256()
    with open(path, "rb") as file:
        for chunk in iter(lambda: file.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _fetch_json(url: str):
    import json
    import urllib.error
    import urllib.request

    try:
        with urllib.request.urlopen(url, timeout=10) as response:
            return json.load(response)
    except (urllib.error.URLError, TimeoutError, OSError, ValueError):
        return None


def release_digest(version: str, filename: str) -> str | None:
    """The sha256 GitHub's release API reports for this asset, or None if it can't be reached."""
    data = _fetch_json(f"{RELEASES_API}/tags/v{version}")
    if data is None:
        return None
    for asset in data.get("assets", []):
        if asset.get("name") == filename:
            digest = asset.get("digest") or ""
            if digest.startswith("sha256:"):
                return digest.removeprefix("sha256:")
    return None


def latest_release_version() -> str | None:
    """The tag of the newest Premake5 release (without its leading 'v'), or None if
    GitHub couldn't be reached."""
    data = _fetch_json(f"{RELEASES_API}/latest")
    if data is None:
        return None
    tag = data.get("tag_name") or ""
    return tag.removeprefix("v") or None


def verify_checksum(path: Path, version: str, filename: str) -> None:
    """Check `path` against pyforge's own pinned hash (when shipped for this version) and
    against GitHub's release digest (when reachable); raise ChecksumError on a mismatch."""
    actual = _sha256(path)
    pinned = KNOWN_HASHES.get(version, {}).get(filename)
    live = release_digest(version, filename)
    if pinned is None and live is None:
        console.print(f"[yellow]⚠️ Could not verify {filename}'s checksum (no pinned hash, and GitHub was unreachable); proceeding unverified.[/yellow]")
        return
    for source, expected in (("pyforge's pinned hash", pinned), ("GitHub's release digest", live)):
        if expected is not None and actual != expected:
            raise ChecksumError(f"{filename}: sha256 is {actual}, but {source} says {expected}")


def install_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION):
    """Download, checksum-verify, and install the given Premake5 version locally."""
    system = platform.system()
    url = premake_url(version, system, platform.machine())

    if url is None:
        console.print(f"[bold red]✗ No Premake5 build for {system}/{platform.machine()}.[/bold red]")
        console.print("  [dim]Set [premake] path in forge.toml to point at a build of your own.[/dim]")
        return False

    filename = url.split("/")[-1]
    archive_path = bin_dir / filename

    console.print(f"[bold blue]📥 Downloading Premake5 v{version}...[/bold blue]")

    bin_dir.mkdir(parents=True, exist_ok=True)

    try:
        _download_with_progress(url, archive_path, filename)
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
    """Force a re-download of `version`, overwriting whatever is currently installed at
    `bin_dir` — unlike ensure_premake(), which leaves an existing install alone."""
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


def ensure_premake(bin_dir: Path, version: str = DEFAULT_PREMAKE_VERSION):
    """
    Ensure the required local Premake5 installation exists.

    Premake is always provided locally (the shared user cache by default). A
    system-wide Premake installation is never used.

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
