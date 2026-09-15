import sys
import subprocess

from .config import BIN_DIR, PROJECT_ROOT, BuildConfig
from .setup.premake import ensure_premake
from .utils import (
    remove_directory,
    run_command,
)


def configure(config: BuildConfig) -> bool:
    """Generate build files with Premake5."""
    print("⚙️ Configuring build...\n")

    premake = ensure_premake()
    if premake is None:
        return False

    command = [
        str(premake),
        config.build_generator,
    ]

    try:
        run_command(command, cwd=PROJECT_ROOT)
        print("✓ Build files generated successfully\n")
        return True

    except subprocess.CalledProcessError as error:
        print("✗ Failed to configure build:")
        if error.stdout:
            print(error.stdout)
        if error.stderr:
            print(error.stderr)
        return False

    except FileNotFoundError:
        print("✗ Premake5 executable could not be run.")
        return False

def init_config() -> bool:
    """Initialize default build configuration."""
    print("⚙️ Initializing build configuration...\n")
    try:
        BuildConfig.init()
        return True
    except Exception as error:
        print(f"✗ Failed to initialize configuration: {error}")
        return False

def build(config: BuildConfig) -> bool:
    """Compile the project for the specified configuration."""
    print(f"🔨 Building project ({config.config})...\n")

    generator = config.build_generator

    if generator == "gmake2":
        # Extract the architecture part from outputdir (e.g. "Debug-macosx-ARM64" -> "arm64")
        arch = config.outputdir.split("-")[-1].lower()
        
        # Target format expected by Premake's Makefile: debug_arm64, release_arm64, etc.
        target_config = f"{config.config}_{arch}"

        command = [
            "make",
            "-C",
            str(PROJECT_ROOT),
            f"config={target_config}",
        ]
    else:
        print(f"✗ Unsupported generator: {generator}")
        return False

    try:
        result = run_command(command)
        if result.stdout:
            print(result.stdout)
        print(f"✓ Build successful ({config.config})\n")
        return True

    except subprocess.CalledProcessError as error:
        print("✗ Build failed:")
        if error.stdout:
            print(error.stdout)
        if error.stderr:
            print(error.stderr)
        return False

    except FileNotFoundError:
        print(f"✗ Build tool not found. Ensure {generator} is installed.")
        return False

def clean() -> None:
    """Clean generated build artifacts."""
    print("🧹 Cleaning build artifacts...\n")

    if BIN_DIR.exists():
        remove_directory(BIN_DIR)
        print(f"✓ Removed {BIN_DIR}")

    print()


def test(config: BuildConfig) -> bool:
    """Run the test executable."""
    print(f"🧪 Running tests ({config.config})...\n")

    # Reconstruct path using BuildConfig: bin/<outputdir>/<test_executable>/<test_executable>
    test_path = config.binary_path / config.test_executable / config.test_executable

    # Fallback check if executable sits directly under outputdir without a subfolder
    if not test_path.exists():
        test_path = config.binary_path / config.test_executable

    if not test_path.exists():
        print(f"✗ Test executable not found at expected path: {test_path}")
        print("  Run 'build build' first.")
        return False

    try:
        result = run_command([str(test_path)])
        if result.stdout:
            print(result.stdout)
        print("✓ Tests passed\n")
        return True

    except subprocess.CalledProcessError as error:
        print("✗ Tests failed:")
        if error.stdout:
            print(error.stdout)
        if error.stderr:
            print(error.stderr)
        return False

    except FileNotFoundError:
        print(f"✗ Could not run {test_path}")
        return False


def all_tasks(config: BuildConfig) -> bool:
    """Configure, build, and test the project."""
    print(f"🚀 Running all build tasks ({config.config})...\n")

    if not configure(config):
        return False

    if not build(config):
        return False

    if not test(config):
        return False

    print("✓ All tasks completed successfully!")
    return True


def print_help() -> None:
    """Print build system usage information."""
    print(
        """
Oryx Build System

Usage:
    build init        Initialize default build configuration file
    build configure   Check dependencies and generate build files
    build build       Generate build files and compile
    build clean       Clean build artifacts
    build test        Run tests
    build all         Configure, build and test
"""
    )


def main() -> int:
    """Main build system entry point."""
    if len(sys.argv) < 2:
        print_help()
        return 0

    command = sys.argv[1].lower()

    if command == "init":
        return 0 if init_config() else 1

    try:
        config = BuildConfig.load()
    except Exception as error:
        print(f"✗ Failed to load build configuration: {error}")
        return 1

    if command == "configure":
        return 0 if configure(config) else 1

    if command == "build":
        if not configure(config):
            return 1
        return 0 if build(config) else 1

    if command == "clean":
        clean()
        return 0

    if command == "test":
        return 0 if test(config) else 1

    if command == "all":
        return 0 if all_tasks(config) else 1

    print(f"Unknown command: {command}")
    print_help()
    return 1

if __name__ == "__main__":
    sys.exit(main())