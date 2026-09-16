from pathlib import Path

from build_system.config import PROJECT_ROOT
from build_system.utils import load_json, merge_by_key, write_json

TASKS_FILE = PROJECT_ROOT / ".vscode" / "tasks.json"
LABEL_PREFIX = "Oryx: "

PROFILES = ["debug", "release", "dist"]


def _profile_compile_tasks() -> list[dict]:
    return [
        {
            "label": f"Compile ({profile.capitalize()})",
            "command": f"uv run build --profile {profile} build compile",
            "group": {"kind": "build", "isDefault": profile == "debug"},
            "problemMatcher": ["$gcc"],
        }
        for profile in PROFILES
    ]


# Cmd+Shift+B ("Run Build Task") is bound to whichever task has
# "isDefault": true — that's always "Compile (Debug)" above, since VS Code
# has no persistent "current configuration" the way Visual Studio's toolbar
# dropdown does. This picker task is the escape hatch for a one-off
# Release/Dist build without switching the Run & Debug profile first: run it
# via Command Palette -> "Tasks: Run Task" -> "Oryx: Compile (choose profile)".
GENERATED_INPUTS = [
    {
        "id": "oryxProfile",
        "type": "pickString",
        "description": "Oryx build profile",
        "options": PROFILES,
        "default": "debug",
    }
]


# Declarative source of truth for the tasks this CLI knows how to generate.
# Each entry maps a task label suffix to the CLI invocation and (optionally)
# a problem matcher / task group.
GENERATED_TASKS = [
    {
        "label": "Configure",
        "command": "uv run build build configure",
        "group": "build",
        "problemMatcher": [],
    },
    *_profile_compile_tasks(),
    {
        "label": "Compile (choose profile)",
        "command": "uv run build --profile ${input:oryxProfile} build compile",
        "group": "build",
        "problemMatcher": ["$gcc"],
    },
    {
        "label": "Test",
        "command": "uv run build test run",
        "group": {"kind": "test", "isDefault": True},
        "problemMatcher": [],
    },
    {
        "label": "Run Oasis",
        "command": "uv run build build run",
        "group": "build",
        "problemMatcher": [],
    },
    {
        "label": "All",
        "command": "uv run build build all",
        "group": "build",
        "problemMatcher": ["$gcc"],
    },
    {
        "label": "Clean",
        "command": "uv run build build clean",
        "group": "build",
        "problemMatcher": [],
    },
]


def _build_task(entry: dict) -> dict:
    return {
        "label": f"{LABEL_PREFIX}{entry['label']}",
        "type": "shell",
        "command": entry["command"],
        "group": entry["group"],
        "problemMatcher": entry["problemMatcher"],
    }


def write_tasks(path: Path = TASKS_FILE) -> Path:
    """Generate or merge .vscode/tasks.json with this CLI's tasks.

    Tasks whose label starts with "Oryx: " are replaced; any other
    user-defined tasks in the file are left untouched.
    """
    generated = [_build_task(entry) for entry in GENERATED_TASKS]

    existing = load_json(path, default={"version": "2.0.0", "tasks": []})
    existing.setdefault("version", "2.0.0")
    existing["tasks"] = merge_by_key(existing.get("tasks", []), generated, key="label")

    return write_json(path, existing)
