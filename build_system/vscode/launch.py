import platform
import shutil
from pathlib import Path

from build_system import legacy_outputs
from build_system.config import RunContext
from build_system.utils import load_json, merge_by_key, write_json
from build_system.vscode.tasks import LABEL_PREFIX, PROFILES

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


def _configuration(profile: str, run: RunContext, debugger: str) -> dict:
    target = run.config.targets[run.config.project.default_target]
    program = legacy_outputs.target_path(run.project.build_dir, profile, run.config.project.name, target.project)
    label = profile.capitalize()

    entry = {
        "name": f"{LABEL_PREFIX}Debug Oasis ({label})",
        "type": DEBUGGER_TYPES[debugger],
        "request": "launch",
        "program": str(program),
        "args": [],
        "cwd": "${workspaceFolder}",
        "preLaunchTask": f"{LABEL_PREFIX}Compile ({label})",
        "console": "internalConsole" if debugger == "lldb" else "integratedTerminal",
    }
    entry.update(DEBUGGER_EXTRA_KEYS[debugger](platform.system()))
    return entry


def write_launch(run: RunContext, debugger: str = "lldb") -> Path:
    """Generate/merge .vscode/launch.json with one debug configuration per
    build profile (Debug/Release/Dist), each wired to its matching
    "Compile (<Profile>)" task (see vscode/tasks.py) as its preLaunchTask —
    giving VS Code's native Run & Debug dropdown a single-button
    "build then debug" flow per profile, the closest match to Visual
    Studio's configuration dropdown without needing a custom extension.
    """
    path = run.project.root / ".vscode" / "launch.json"
    generated = [_configuration(profile, run, debugger) for profile in PROFILES]

    existing = load_json(path, default={"version": "0.2.0", "configurations": []})
    existing.setdefault("version", "0.2.0")
    existing["configurations"] = merge_by_key(existing.get("configurations", []), generated, key="name")

    return write_json(path, existing)
