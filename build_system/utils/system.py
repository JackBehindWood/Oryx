import platform
import subprocess


def get_os():
    """Return the current operating system."""
    return platform.system()


def get_architecture():
    """Return the current machine architecture."""
    return platform.machine()


def get_macos_sdk_path():
    """Resolve the active Xcode/CLT macOS SDK path via `xcrun`, or None if
    unavailable (not on macOS, or no toolchain installed). Real clang
    invocations auto-detect this SDK, but tools that just parse a compile
    command's argument list (VS Code's C/C++ extension, clangd) need it
    passed explicitly via -isysroot."""
    try:
        result = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def run_command(command, cwd=None, capture_output=True, env=None):
    """
    Run a command and return the completed subprocess result.

    Raises:
        subprocess.CalledProcessError: If the command fails.
        FileNotFoundError: If the executable cannot be found.
    """
    return subprocess.run(
        command,
        cwd=str(cwd) if cwd else None,
        capture_output=capture_output,
        env=env,
        text=True,
        check=True,
    )


def missing_module_hint(module: str, uv_group: str, pip_packages: list[str]) -> str | None:
    """None when `module` is importable by this Python, else how to install it with uv or pip."""
    import importlib.util
    import sys

    if importlib.util.find_spec(module) is not None:
        return None
    return (
        f"{module} is not installed for {sys.executable}. Install it with `uv sync --group {uv_group}` "
        f"(or run forge via `uv run --group {uv_group} forge …`), or `{sys.executable} -m pip install {' '.join(pip_packages)}`."
    )
