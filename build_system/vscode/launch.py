import dataclasses
import platform
import shutil
from pathlib import Path

from build_system.config import BuildConfig, LocalConfig, PROJECT_ROOT, script_search_path
from build_system.utils import load_json, merge_by_key, write_json
from build_system.vscode.tasks import LABEL_PREFIX, PROFILES

LAUNCH_FILE = PROJECT_ROOT / ".vscode" / "launch.json"

# The one-line switch for a different debugger: add/replace an entry here.
# "type" is VS Code's own launch.json debug-adapter type.
DEBUGGER_TYPES = {
    "lldb": "lldb",  # requires the vadimcn.vscode-lldb ("CodeLLDB") extension
    "cppdbg": "cppdbg",  # requires the ms-vscode.cpptools ("C/C++") extension
}


def _lldb_extra_keys(system: str) -> dict:
    return {}


def _cppdbg_extra_keys(system: str) -> dict:
    return {
        "MIMode": "lldb" if system == "Darwin" else "gdb",
        "miDebuggerPath": shutil.which("lldb-mi" if system == "Darwin" else "gdb") or "",
    }


DEBUGGER_EXTRA_KEYS = {"lldb": _lldb_extra_keys, "cppdbg": _cppdbg_extra_keys}


def _configuration(profile: str, cfg: BuildConfig, debugger: str) -> dict:
    # Recompute cfg.executable_path("oasis") as if cfg.profile were this
    # profile, so all three Debug/Release/Dist launch entries can be
    # generated from a single `config init` run — reuses BuildConfig's own
    # outputdir/Premake-quirk resolution (config.py), no path guessing here.
    profile_cfg = dataclasses.replace(cfg, profile=profile)
    label = profile.capitalize()

    entry = {
        "name": f"{LABEL_PREFIX}Debug Oasis ({label})",
        "type": DEBUGGER_TYPES[debugger],
        "request": "launch",
        "program": str(profile_cfg.executable_path("oasis")),
        "args": [],
        "env": {"ORYX_SCRIPT_PATH": script_search_path(cfg, LocalConfig.load())},
        "cwd": "${workspaceFolder}",
        "preLaunchTask": f"{LABEL_PREFIX}Compile ({label})",
        "console": "internalConsole" if debugger == "lldb" else "integratedTerminal",
    }
    entry.update(DEBUGGER_EXTRA_KEYS[debugger](platform.system()))
    return entry


def write_launch(cfg: BuildConfig, debugger: str = "lldb", path: Path = LAUNCH_FILE) -> Path:
    """Generate/merge .vscode/launch.json with one debug configuration per
    build profile (Debug/Release/Dist), each wired to its matching
    "Compile (<Profile>)" task (see vscode/tasks.py) as its preLaunchTask —
    giving VS Code's native Run & Debug dropdown a single-button
    "build then debug" flow per profile, the closest match to Visual
    Studio's configuration dropdown without needing a custom extension.
    """
    generated = [_configuration(profile, cfg, debugger) for profile in PROFILES]

    existing = load_json(path, default={"version": "0.2.0", "configurations": []})
    existing.setdefault("version", "0.2.0")
    existing["configurations"] = merge_by_key(existing.get("configurations", []), generated, key="name")

    return write_json(path, existing)
