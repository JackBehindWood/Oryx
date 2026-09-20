import platform
import shutil
import subprocess
from pathlib import Path

from build_system.compile_commands import COMPILE_COMMANDS_FILE
from build_system.config import BuildConfig, PROJECT_ROOT
from build_system.utils import get_macos_sdk_path, load_json, merge_by_key, write_json
from build_system.vendor import vendor_include_paths

C_CPP_PROPERTIES_FILE = PROJECT_ROOT / ".vscode" / "c_cpp_properties.json"

COMPILE_COMMANDS_TOKEN = f"${{workspaceFolder}}/{COMPILE_COMMANDS_FILE.relative_to(PROJECT_ROOT).as_posix()}"

# Mirrors the `filter "configurations:<Profile>" defines { ... }` blocks in
# premake5.lua. Used only as a fallback (see FALLBACK_INCLUDE_PATHS below).
PROFILE_DEFINES = {
    "debug": ["OX_DEBUG", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING"],
    "release": ["OX_RELEASE", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING"],
    "dist": ["OX_DIST"],
}

# Fallback only, for before `build build configure` has ever run (or if
# compile_commands.json is otherwise missing). Real per-project accuracy comes
# from compile_commands.json — generated straight from Premake's own resolved
# build output — via the "compileCommands" field below, which VS Code prefers
# over these lists whenever it's present. This union just keeps IntelliSense
# from being completely broken on a fresh checkout.
FALLBACK_INCLUDE_PATHS = [
    "${workspaceFolder}/Oryx/src",
    "${workspaceFolder}/Oasis/src",
    "${workspaceFolder}/tests",
]


def _fallback_include_paths() -> list[str]:
    # Appends every <project>/vendor/<lib>/ dir discovered on disk (see
    # build_system/vendor.py) instead of hardcoding doctest's path here.
    return FALLBACK_INCLUDE_PATHS + vendor_include_paths()


def _defines(cfg: BuildConfig) -> list[str]:
    return PROFILE_DEFINES.get(cfg.profile, [f"OX_{cfg.profile.upper()}"])


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


def _macos_configuration(name: str, intellisense_mode: str, cfg: BuildConfig) -> dict:
    sdk = get_macos_sdk_path()
    sdk_includes = [f"{sdk}/usr/include/c++/v1", f"{sdk}/usr/include"] if sdk else []
    include_paths = _fallback_include_paths()

    return {
        "name": name,
        "includePath": include_paths + sdk_includes + [_homebrew_include_path(intellisense_mode)],
        "defines": _defines(cfg),
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


def _linux_configuration(cfg: BuildConfig) -> dict:
    compiler_path = shutil.which("g++") or shutil.which("clang++") or "/usr/bin/g++"
    include_paths = _fallback_include_paths()

    return {
        "name": "Linux",
        "includePath": include_paths + ["/usr/include", "/usr/local/include"],
        "defines": _defines(cfg),
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


def _windows_configuration(cfg: BuildConfig) -> dict:
    include_paths = _fallback_include_paths()

    return {
        "name": "Windows",
        "includePath": include_paths,
        "defines": _defines(cfg),
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


def _generate_configurations(cfg: BuildConfig) -> list[dict]:
    """Generate IntelliSense configurations for the host platform only —
    we don't guess SDK/compiler paths for platforms we're not running on."""
    system = platform.system()
    if system == "Darwin":
        return [
            _macos_configuration("Mac ARM64", "macos-clang-arm64", cfg),
            _macos_configuration("Mac x64", "macos-clang-x64", cfg),
        ]
    if system == "Linux":
        return [_linux_configuration(cfg)]
    if system == "Windows":
        return [_windows_configuration(cfg)]
    return []


def write_c_cpp_properties(cfg: BuildConfig, path: Path = C_CPP_PROPERTIES_FILE) -> Path:
    """Generate or merge .vscode/c_cpp_properties.json for the host platform.

    Configurations are matched and replaced by name; any other user-defined
    configuration in the file is left untouched. Each configuration points
    "compileCommands" at compile_commands.json (generated by `build build
    configure` from Premake's own resolved output — see
    build_system/compile_commands.py), which VS Code prefers over
    includePath/defines whenever it exists. Those lists remain only as a
    fallback for before compile_commands.json exists.
    """
    generated = _generate_configurations(cfg)

    existing = load_json(path, default={"version": 4, "configurations": []})
    existing.setdefault("version", 4)
    existing["configurations"] = merge_by_key(existing.get("configurations", []), generated, key="name")

    return write_json(path, existing)
