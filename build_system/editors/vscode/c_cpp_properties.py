import platform
import shutil
import subprocess
from pathlib import Path

from build_system import workspace
from build_system.compile_commands import COMPILE_COMMANDS_NAME
from build_system.config import RunContext
from build_system.utils import get_macos_sdk_path, load_json, merge_by_key, write_json

COMPILE_COMMANDS_TOKEN = f"${{workspaceFolder}}/build/{COMPILE_COMMANDS_NAME}"


def _workspace_path(root: Path, path: Path) -> str:
    return f"${{workspaceFolder}}/{path.relative_to(root).as_posix()}" if path.is_relative_to(root) else path.as_posix()


def _unique(items) -> list:
    return list(dict.fromkeys(items))


def _include_paths(run: RunContext) -> list[str]:
    """Union of every exported project's include dirs for the active profile."""
    ws = workspace.require(run.project)
    root = run.project.root
    return _unique(_workspace_path(root, path) for name in ws.projects for path in ws.config(name, run.profile).includedirs)


def _defines(run: RunContext) -> list[str]:
    ws = workspace.require(run.project)
    return _unique(define for name in ws.projects for define in ws.config(name, run.profile).defines)


def _macos_compiler_path() -> str:
    try:
        result = subprocess.run(["xcrun", "--find", "clang++"], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return shutil.which("clang++") or "/usr/bin/clang++"


def _homebrew_include_path(intellisense_mode: str) -> str:
    # Apple Silicon Homebrew installs to /opt/homebrew; Intel Homebrew (and
    # the historical macOS default) uses /usr/local — /usr/local/include
    # doesn't exist at all on an ARM64 Homebrew install.
    return "/opt/homebrew/include" if "arm64" in intellisense_mode else "/usr/local/include"


def _macos_configuration(name: str, intellisense_mode: str, run: RunContext) -> dict:
    sdk = get_macos_sdk_path()
    sdk_includes = [f"{sdk}/usr/include/c++/v1", f"{sdk}/usr/include"] if sdk else []
    include_paths = _include_paths(run)

    return {
        "name": name,
        "includePath": include_paths + sdk_includes + [_homebrew_include_path(intellisense_mode)],
        "defines": _defines(run),
        "macFrameworkPath": [f"{sdk}/System/Library/Frameworks"] if sdk else [],
        "compilerPath": _macos_compiler_path(),
        "compileCommands": COMPILE_COMMANDS_TOKEN,
        "cStandard": "c17",
        "cppStandard": "c++20",
        "intelliSenseMode": intellisense_mode,
        "browse": {
            "path": ["${workspaceFolder}"] + include_paths,
            "limitSymbolsToIncludedHeaders": True,
        },
    }


def _linux_configuration(run: RunContext) -> dict:
    compiler_path = shutil.which("g++") or shutil.which("clang++") or "/usr/bin/g++"
    include_paths = _include_paths(run)

    return {
        "name": "Linux",
        "includePath": include_paths + ["/usr/include", "/usr/local/include"],
        "defines": _defines(run),
        "compilerPath": compiler_path,
        "compileCommands": COMPILE_COMMANDS_TOKEN,
        "cStandard": "c17",
        "cppStandard": "c++20",
        "intelliSenseMode": "linux-gcc-x64",
        "browse": {
            "path": ["${workspaceFolder}"] + include_paths,
            "limitSymbolsToIncludedHeaders": True,
        },
    }


def _windows_configuration(run: RunContext) -> dict:
    include_paths = _include_paths(run)

    return {
        "name": "Windows",
        "includePath": include_paths,
        "defines": _defines(run),
        # Left as a bare name: cpptools resolves this itself via its own MSVC
        # detection (vswhere), which is more reliable than us guessing a path.
        "compilerPath": "cl.exe",
        "compileCommands": COMPILE_COMMANDS_TOKEN,
        "cStandard": "c17",
        "cppStandard": "c++20",
        "intelliSenseMode": "windows-msvc-x64",
        "browse": {
            "path": ["${workspaceFolder}"] + include_paths,
            "limitSymbolsToIncludedHeaders": True,
        },
    }


def _generate_configurations(run: RunContext) -> list[dict]:
    """Generate IntelliSense configurations for the host platform only —
    we don't guess SDK/compiler paths for platforms we're not running on."""
    system = platform.system()
    if system == "Darwin":
        return [
            _macos_configuration("Mac ARM64", "macos-clang-arm64", run),
            _macos_configuration("Mac x64", "macos-clang-x64", run),
        ]
    if system == "Linux":
        return [_linux_configuration(run)]
    if system == "Windows":
        return [_windows_configuration(run)]
    return []


def write_c_cpp_properties(run: RunContext) -> Path:
    """Generate or merge .vscode/c_cpp_properties.json for the host platform, replacing configurations by name.

    VS Code prefers "compileCommands" (per-file flags) whenever that file exists; the includePath/defines
    unions from the Premake export cover files it doesn't list.
    """
    path = run.project.root / ".vscode" / "c_cpp_properties.json"
    generated = _generate_configurations(run)

    existing = load_json(path, default={"version": 4, "configurations": []})
    existing.setdefault("version", 4)
    existing["configurations"] = merge_by_key(existing.get("configurations", []), generated, key="name")

    return write_json(path, existing)
