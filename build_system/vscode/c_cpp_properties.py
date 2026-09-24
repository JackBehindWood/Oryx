import platform
import shutil
import subprocess
from pathlib import Path

from build_system.compile_commands import COMPILE_COMMANDS_NAME
from build_system.config import RunContext
from build_system.setup.python_env import PythonEnvError, python_build_info
from build_system.utils import get_macos_sdk_path, load_json, merge_by_key, write_json
from build_system.vendor import vendor_include_paths

COMPILE_COMMANDS_TOKEN = f"${{workspaceFolder}}/build/{COMPILE_COMMANDS_NAME}"

# Mirrors the `filter "configurations:<Profile>" defines { ... }` blocks in
# premake5.lua. Used only as a fallback (see FALLBACK_INCLUDE_PATHS below).
PROFILE_DEFINES = {
    "debug": ["OX_DEBUG", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING"],
    "release": ["OX_RELEASE", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING"],
    "dist": ["OX_DIST"],
}

# Mirrors the always-on defines in the projects' premake5.lua files.
BASE_DEFINES = ["SPDLOG_COMPILED_LIB"]

# Fallback only, for before `forge build configure` has ever run (or if
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

# Private Python backend (Oryx/backends/Python), compiled into Oryx only when Python is on.
PYTHON_BACKEND_INCLUDE_PATH = "${workspaceFolder}/Oryx/backends/Python"


def _python_enabled(run: RunContext) -> bool:
    return run.options.get("python", False)


def _python_build_info(run: RunContext):
    # None when Python is off, or when no usable interpreter is found: the IDE fallback simply omits the CPython bits.
    if not _python_enabled(run):
        return None
    try:
        return python_build_info()
    except PythonEnvError:
        return None


def _fallback_include_paths(run: RunContext) -> list[str]:
    # Appends every <project>/vendor/<lib>/ header dir discovered on disk (see
    # build_system/vendor.py) instead of hardcoding each library's path here.
    paths = FALLBACK_INCLUDE_PATHS + vendor_include_paths(run.project.root, _python_enabled(run))
    if _python_enabled(run):
        paths.append(PYTHON_BACKEND_INCLUDE_PATH)
        paths.append("${workspaceFolder}/build/generated")
        info = _python_build_info(run)
        if info is not None:
            paths.append(info.include_dir.as_posix())
    return paths


def _defines(run: RunContext) -> list[str]:
    defines = BASE_DEFINES + PROFILE_DEFINES.get(run.profile, [f"OX_{run.profile.upper()}"])
    if _python_enabled(run):
        defines.append("OX_ENABLE_PYTHON")
    return defines


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
    include_paths = _fallback_include_paths(run)

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
    include_paths = _fallback_include_paths(run)

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
    include_paths = _fallback_include_paths(run)

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
    """Generate or merge .vscode/c_cpp_properties.json for the host platform.

    Configurations are matched and replaced by name; any other user-defined
    configuration in the file is left untouched. Each configuration points
    "compileCommands" at compile_commands.json (generated by `forge build
    configure` from Premake's own resolved output — see
    build_system/compile_commands.py), which VS Code prefers over
    includePath/defines whenever it exists. Those lists remain only as a
    fallback for before compile_commands.json exists.
    """
    path = run.project.root / ".vscode" / "c_cpp_properties.json"
    generated = _generate_configurations(run)

    existing = load_json(path, default={"version": 4, "configurations": []})
    existing.setdefault("version", 4)
    existing["configurations"] = merge_by_key(existing.get("configurations", []), generated, key="name")

    return write_json(path, existing)
