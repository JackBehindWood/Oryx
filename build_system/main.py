import subprocess
import sys

from .config import BIN_DIR, PROJECT_ROOT, load_config
from .setup.premake import ensure_premake
from .utils import (
    remove_directory,
    run_command,
)

def configure(config):
    """Generate build files with Premake5."""
    print("⚙️ Configuring build...\n")

    premake = ensure_premake()

    if premake is None:
        return False

    generator = config.get("build_generator", "gmake2")

    command = [
        str(premake),
        generator,
    ]

    try:
        run_command(
            command,
            cwd=PROJECT_ROOT,
        )

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


def build(config):
    """Compile the project."""
    print("🔨 Building project...\n")

    generator = config.get("build_generator", "gmake2")

    if generator == "gmake2":
        command = [
            "make",
            "-C",
            str(PROJECT_ROOT),
        ]

    else:
        print(f"✗ Unsupported generator: {generator}")
        return False

    try:
        result = run_command(command)

        if result.stdout:
            print(result.stdout)

        print("✓ Build successful\n")
        return True

    except subprocess.CalledProcessError as error:
        print("✗ Build failed:")

        if error.stdout:
            print(error.stdout)

        if error.stderr:
            print(error.stderr)

        return False

    except FileNotFoundError:
        print(
            f"✗ Build tool not found. "
            f"Ensure {generator} is installed."
        )
        return False


def clean():
    """Clean generated build artifacts."""
    print("🧹 Cleaning build artifacts...\n")

    if BIN_DIR.exists():
        remove_directory(BIN_DIR)
        print(f"✓ Removed {BIN_DIR}")

    print()


def test(config):
    """Run the test executable."""
    print("🧪 Running tests...\n")

    test_executable = config.get(
        "test_executable",
        "oryx_tests",
    )

    test_path = BIN_DIR / test_executable

    if not test_path.exists():
        print(f"✗ Test executable not found: {test_path}")
        print("  Run 'build build' first.")
        return False

    try:
        result = run_command(
            [str(test_path)]
        )

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


def all_tasks(config):
    """Configure, build and test the project."""
    print("🚀 Running all build tasks...\n")

    if not configure(config):
        return False

    if not build(config):
        return False

    if not test(config):
        return False

    print("✓ All tasks completed successfully!")
    return True


def print_help():
    """Print build system usage information."""
    print(
        """
Oryx Build System

Usage:
    build configure   Check dependencies and generate build files
    build build       Generate build files and compile
    build clean       Clean build artifacts
    build test        Run tests
    build all         Configure, build and test
"""
    )


def main():
    """Main build system entry point."""
    if len(sys.argv) < 2:
        print_help()
        return 0

    command = sys.argv[1].lower()
    config = load_config()

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
    print("Run 'build' for usage information.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
