import platform
import subprocess

from ..config import PREMAKE_DIR
from .utils import (
    download_file,
    extract_archive,
    make_executable,
    remove_file,
)

# ---------------------------------------------------------------------------
# Premake configuration
# ---------------------------------------------------------------------------

PREMAKE_VERSION = "5.0.0-beta2"

PREMAKE_URLS = {
    "Linux": (
        "https://github.com/premake/premake-core/releases/"
        "download/v5.0.0-beta2/"
        "premake-5.0.0-beta2-linux.tar.gz"
    ),
    "Darwin": (
        "https://github.com/premake/premake-core/releases/"
        "download/v5.0.0-beta2/"
        "premake-5.0.0-beta2-macosx.tar.gz"
    ),
    "Windows": (
        "https://github.com/premake/premake-core/releases/"
        "download/v5.0.0-beta2/"
        "premake-5.0.0-beta2-windows.zip"
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

def install_premake():
    """Download and install the required Premake5 version locally."""
    system = platform.system()

    if system not in PREMAKE_URLS:
        print(f"✗ Unsupported operating system: {system}")
        return False

    url = PREMAKE_URLS[system]

    print(f"📥 Downloading Premake5 v{PREMAKE_VERSION}...\n")
    print(f"⏳ Downloading from {url}...")

    PREMAKE_DIR.mkdir(parents=True, exist_ok=True)

    filename = url.split("/")[-1]
    archive_path = PREMAKE_DIR / filename

    try:
        # Download Premake
        download_file(url, archive_path)
        print(f"✓ Downloaded {filename}\n")

        # Extract Premake
        print(f"📦 Extracting {filename}...")
        extract_archive(archive_path, PREMAKE_DIR)
        print("✓ Extracted successfully\n")

        # Remove downloaded archive
        remove_file(archive_path)

        # Make executable on Unix-like systems
        if system in {"Linux", "Darwin"}:
            executable = get_premake_executable()

            if executable.exists():
                make_executable(executable)
                print(f"✓ Made executable: {executable}\n")

        # Download Premake licence
        print("📄 Downloading Premake5 licence...")

        license_path = PREMAKE_DIR / "LICENSE.txt"

        try:
            download_file(PREMAKE_LICENSE_URL, license_path)
            print(f"✓ Saved licence to {license_path}\n")

        except Exception as error:
            print(f"⚠️ Could not download licence: {error}\n")

        return True

    except Exception as error:
        print(f"✗ Failed to download/extract Premake5: {error}")

        # Clean up incomplete archive if necessary
        remove_file(archive_path)

        return False


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
        print(f"✓ premake5: found locally at {executable}\n")
        return executable

    print("✗ premake5: local installation not found.")
    print(f"  Expected: {executable}\n")

    print("📦 Installing Premake5 locally...\n")

    if install_premake():
        executable = get_premake_executable()

        if check_local_premake():
            print(f"✓ premake5: installed locally at {executable}\n")
            return executable

    print("✗ Could not install the required local Premake5.")
    return None