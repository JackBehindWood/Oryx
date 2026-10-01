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
