import platform
import shutil
from pathlib import Path

from pyforge import workspace
from pyforge.config import RunContext
from pyforge.utils import load_json, merge_by_key, write_json
from pyforge.editors.vscode.tasks import PROFILES, label_prefix

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


def _configuration(run: RunContext, name: str, program: Path, profile: str, debugger: str) -> dict:
    prefix = label_prefix(run)
    label = profile.capitalize()
    entry = {
        "name": f"{prefix}Debug {name} ({label})",
        "type": DEBUGGER_TYPES[debugger],
        "request": "launch",
        "program": str(program),
        "args": [],
        "cwd": "${workspaceFolder}",
        "preLaunchTask": f"{prefix}Compile ({label})",
        "console": "internalConsole" if debugger == "lldb" else "integratedTerminal",
    }
    entry.update(DEBUGGER_EXTRA_KEYS[debugger](platform.system()))
    return entry


def write_launch(run: RunContext, debugger: str = "lldb") -> Path:
    """Generate/merge .vscode/launch.json: one debug entry per [targets] entry and profile, each
    building first through its "Compile (<Profile>)" task."""
    path = run.project.root / ".vscode" / "launch.json"
    ws = workspace.require(run.project)
    generated = [
        _configuration(run, name, ws.target_path(target.project, profile), profile, debugger)
        for name, target in run.config.targets.items()
        for profile in PROFILES
    ]

    existing = load_json(path, default={"version": "0.2.0", "configurations": []})
    existing.setdefault("version", "0.2.0")
    existing["configurations"] = merge_by_key(existing.get("configurations", []), generated, key="name")

    return write_json(path, existing)
