from pathlib import Path

from rich.console import Console

from .assets import KNOWN_HASHES, RELEASES_API

console = Console()


class ChecksumError(RuntimeError):
    pass


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
