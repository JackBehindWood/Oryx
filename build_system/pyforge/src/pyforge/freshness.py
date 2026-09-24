"""Stat-based check for whether the generated build files are stale, backed by build/forge/stamp.json."""

import json
from dataclasses import dataclass, field
from pathlib import Path

from .config.load import LEGACY_LOCAL_CONFIG_NAME, LOCAL_CONFIG_NAME
from .project import Project
from .workspace import Workspace

STAMP_NAME = "stamp.json"
MISSING = -1


@dataclass(frozen=True)
class Stamp:
    mtimes: dict[str, int] = field(default_factory=dict)
    options_hash: str = ""
    sources: dict[str, list[str]] = field(default_factory=dict)


def stamp_file(project: Project) -> Path:
    return project.forge_dir / STAMP_NAME


def _mtime(path: Path) -> int:
    try:
        return path.stat().st_mtime_ns
    except OSError:
        return MISSING


def watched_paths(project: Project, workspace: Workspace) -> list[Path]:
    """Directories whose entries Premake globbed, every Lua script it loaded, and the forge config files."""
    configs = {project.config_file, project.path(LOCAL_CONFIG_NAME), project.path(LEGACY_LOCAL_CONFIG_NAME), project.path(".gitmodules")}
    return sorted(workspace.source_dirs() | set(workspace.scripts) | configs)


def load_stamp(project: Project) -> Stamp | None:
    path = stamp_file(project)
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return Stamp(mtimes=data["mtimes"], options_hash=data.get("options_hash", ""), sources=data.get("sources", {}))
    except (json.JSONDecodeError, KeyError, TypeError):
        return None


def record(project: Project, workspace: Workspace, options_hash: str = "") -> Stamp:
    stamp = Stamp(
        mtimes={str(path): _mtime(path) for path in watched_paths(project, workspace)},
        options_hash=options_hash,
        sources={name: [str(file) for file in ws_project.sources] for name, ws_project in workspace.projects.items()},
    )
    path = stamp_file(project)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"mtimes": stamp.mtimes, "options_hash": stamp.options_hash, "sources": stamp.sources}), encoding="utf-8")
    return stamp


def _describe(project: Project, path: str) -> str:
    try:
        return Path(path).relative_to(project.root).as_posix() or "."
    except ValueError:
        return path


def stale(project: Project, options_hash: str | None = None) -> str | None:
    """Why the build files must be regenerated, or None when the last configure is still current."""
    stamp = load_stamp(project)
    if stamp is None:
        return "no previous configure"
    if options_hash is not None and stamp.options_hash != options_hash:
        return "build options changed"
    for path, recorded in stamp.mtimes.items():
        if _mtime(Path(path)) != recorded:
            return f"{_describe(project, path)} changed"
    return None


def projects_that_lost_sources(previous: Stamp | None, workspace: Workspace) -> list[str]:
    """Projects that compiled a source file last configure which is now unlisted or, if listed by explicit path, deleted."""
    if previous is None:
        return []
    lost = set()
    for name, files in previous.sources.items():
        ws_project = workspace.projects.get(name)
        current = {str(file) for file in ws_project.sources} if ws_project else set()
        if set(files) - current:
            lost.add(name)
    for name, ws_project in workspace.projects.items():
        if any(not file.exists() for file in ws_project.sources):
            lost.add(name)
    return sorted(lost)
