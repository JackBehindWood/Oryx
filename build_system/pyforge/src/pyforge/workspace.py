"""Reader for build/forge/workspace.json, the Premake export written by premake/forge.lua."""

import json
import platform
from dataclasses import dataclass
from pathlib import Path

from .project import Project

SUPPORTED_FORMAT = 1
SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".m", ".mm"})
_ARM_ARCHITECTURES = frozenset({"arm64", "aarch64"})


class WorkspaceError(RuntimeError):
    pass


@dataclass(frozen=True)
class WsConfig:
    buildcfg: str
    platform: str
    architecture: str
    kind: str
    token: str
    target: Path
    targetdir: Path
    objdir: Path
    defines: tuple[str, ...]
    includedirs: tuple[Path, ...]


@dataclass(frozen=True)
class WsProject:
    name: str
    basedir: Path
    script: Path
    configs: tuple[WsConfig, ...]
    files: tuple[Path, ...]

    @property
    def sources(self) -> tuple[Path, ...]:
        return tuple(file for file in self.files if file.suffix in SOURCE_SUFFIXES)


@dataclass(frozen=True)
class Workspace:
    location: Path
    action: str
    scripts: tuple[Path, ...]
    projects: dict[str, WsProject]

    def project(self, name: str) -> WsProject:
        try:
            return self.projects[name]
        except KeyError:
            available = ", ".join(sorted(self.projects)) or "(none)"
            raise WorkspaceError(f"Premake project '{name}' not found in the workspace. Available: {available}") from None

    def config(self, project: str, profile: str) -> WsConfig:
        return select_config(self.project(project).configs, profile)

    def target_path(self, project: str, profile: str) -> Path:
        return self.config(project, profile).target

    def token(self, profile: str) -> str:
        first = next(iter(self.projects.values()), None)
        if first is None:
            raise WorkspaceError("The Premake workspace has no projects.")
        return select_config(first.configs, profile).token

    def source_dirs(self) -> set[Path]:
        """Every directory holding a project file, plus its ancestors up to the project's basedir."""
        dirs: set[Path] = set()
        for project in self.projects.values():
            for file in project.files:
                directory = file.parent
                while directory not in dirs:
                    dirs.add(directory)
                    if directory == project.basedir or project.basedir not in directory.parents:
                        break
                    directory = directory.parent
        return dirs


def _host_is_arm() -> bool:
    return platform.machine().lower() in _ARM_ARCHITECTURES


def select_config(configs: tuple[WsConfig, ...], profile: str) -> WsConfig:
    """The config for `profile`, preferring the platform that matches the host architecture."""
    matching = [cfg for cfg in configs if cfg.buildcfg.lower() == profile.lower()]
    if not matching:
        available = ", ".join(sorted({cfg.buildcfg for cfg in configs})) or "(none)"
        raise WorkspaceError(f"No '{profile}' configuration in the Premake workspace. Available: {available}")
    host_arm = _host_is_arm()
    for cfg in matching:
        if (cfg.architecture.lower() in _ARM_ARCHITECTURES) == host_arm:
            return cfg
    return matching[0]


def _as_list(value) -> list:
    return [] if value in (None, {}) else list(value)


def _config(data: dict) -> WsConfig:
    return WsConfig(
        buildcfg=data["buildcfg"],
        platform=data.get("platform", ""),
        architecture=data.get("architecture", ""),
        kind=data["kind"],
        token=data["token"],
        target=Path(data["target"]),
        targetdir=Path(data["targetdir"]),
        objdir=Path(data["objdir"]),
        defines=tuple(_as_list(data.get("defines"))),
        includedirs=tuple(Path(path) for path in _as_list(data.get("includedirs"))),
    )


def _project(name: str, data: dict) -> WsProject:
    return WsProject(
        name=name,
        basedir=Path(data["basedir"]),
        script=Path(data["script"]),
        configs=tuple(_config(cfg) for cfg in _as_list(data.get("configs"))),
        files=tuple(Path(file) for file in _as_list(data.get("files"))),
    )


def parse(data: dict) -> Workspace:
    if data.get("format") != SUPPORTED_FORMAT:
        raise WorkspaceError(f"Unsupported workspace.json format {data.get('format')!r}; run `forge build configure` again.")
    return Workspace(
        location=Path(data["location"]),
        action=data.get("action", ""),
        scripts=tuple(Path(script) for script in _as_list(data.get("scripts"))),
        projects={name: _project(name, project) for name, project in (data.get("projects") or {}).items()},
    )


def workspace_file(project: Project) -> Path:
    return project.forge_dir / "workspace.json"


def load(project: Project) -> Workspace | None:
    """The last export, or None before the first `forge build configure`."""
    path = workspace_file(project)
    if not path.is_file():
        return None
    return parse(json.loads(path.read_text(encoding="utf-8")))


def require(project: Project) -> Workspace:
    workspace = load(project)
    if workspace is None:
        raise WorkspaceError("No Premake export yet; run `forge build configure` first.")
    return workspace
