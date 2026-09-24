from pathlib import Path

from pyforge.config import Profile, RunContext
from pyforge.utils import load_json, merge_by_key, write_json

PROFILES = [str(profile) for profile in Profile]
PROFILE_INPUT = "forgeProfile"


def label_prefix(run: RunContext) -> str:
    return f"{run.config.project.name}: "


def forge_command(root: Path) -> str:
    return "uv run forge" if (root / "uv.lock").is_file() else "forge"


def _task(label: str, command: str, group, problem_matcher: list[str]) -> dict:
    return {"label": label, "command": command, "group": group, "problemMatcher": problem_matcher}


def generated_tasks(run: RunContext) -> list[dict]:
    """Tasks this CLI owns; Cmd+Shift+B runs the one with "isDefault" (Compile (Debug))."""
    forge = forge_command(run.project.root)
    return [
        _task("Configure", f"{forge} build configure", "build", []),
        *(
            _task(f"Compile ({profile.capitalize()})", f"{forge} --profile {profile} build compile", {"kind": "build", "isDefault": profile == "debug"}, ["$gcc"])
            for profile in PROFILES
        ),
        _task("Compile (choose profile)", f"{forge} --profile ${{input:{PROFILE_INPUT}}} build compile", "build", ["$gcc"]),
        _task("Test", f"{forge} test run", {"kind": "test", "isDefault": True}, []),
        *(_task(f"Run {name}", f"{forge} build run {name}", "build", []) for name in run.config.targets),
        _task("All", f"{forge} build all", "build", ["$gcc"]),
        _task("Clean", f"{forge} build clean", "build", []),
    ]


def _build_task(prefix: str, entry: dict) -> dict:
    return {
        "label": f"{prefix}{entry['label']}",
        "type": "shell",
        "command": entry["command"],
        "group": entry["group"],
        "problemMatcher": entry["problemMatcher"],
    }


def _profile_input(run: RunContext) -> dict:
    return {
        "id": PROFILE_INPUT,
        "type": "pickString",
        "description": f"{run.config.project.name} build profile",
        "options": PROFILES,
        "default": str(run.config.build.default_profile),
    }


def write_tasks(run: RunContext) -> Path:
    """Generate or merge .vscode/tasks.json; tasks labelled "<project name>: …" are replaced, others kept."""
    path = run.project.root / ".vscode" / "tasks.json"
    prefix = label_prefix(run)
    generated = [_build_task(prefix, entry) for entry in generated_tasks(run)]

    existing = load_json(path, default={"version": "2.0.0", "tasks": []})
    existing.setdefault("version", "2.0.0")
    existing["tasks"] = merge_by_key(existing.get("tasks", []), generated, key="label")
    existing["inputs"] = merge_by_key(existing.get("inputs", []), [_profile_input(run)], key="id")

    return write_json(path, existing)
