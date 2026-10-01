"""forge.toml [dependencies] → resolved paths, requirement filtering, and build/forge/config.json for Premake."""

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from .. import cache
from ..config import Dependency, FetchMode, ForgeConfig, RunContext
from ..project import Project

PREMAKE_CONFIG_NAME = "config.json"
PREMAKE_CONFIG_FORMAT = 1


class DependencyError(RuntimeError):
    pass


@dataclass(frozen=True)
class ResolvedDependency:
    name: str
    spec: Dependency
    dir: Path | None
    root: Path | None = None

    @property
    def include(self) -> Path | None:
        if self.dir is None:
            return None
        return self.dir / self.spec.include if self.spec.include else self.dir

    @property
    def sources(self) -> Path | None:
        return self.dir / self.spec.sources if self.dir is not None and self.spec.sources else None

    @property
    def present(self) -> bool:
        if self.dir is None:
            return self.flags() is not None
        return self.dir.is_dir() and any(self.dir.iterdir())

    def flags(self) -> tuple[list[str], list[str]] | None:
        """(cflags, libs) of a system dependency, or None when it isn't found."""
        from .sources.system import SystemSource

        try:
            return SystemSource().probe(self.root or Path.cwd(), self)
        except DependencyError:
            return None


def cache_pin(spec: Dependency) -> str:
    if spec.source == "git":
        return spec.commit
    if spec.source in ("archive", "file"):
        return spec.sha256[:12]
    return ""


def dependency_dir(project: Project, cfg: ForgeConfig, name: str, spec: Dependency) -> Path | None:
    if spec.source == "system":
        return None
    if spec.source in ("git", "archive", "file"):
        return cache.deps_dir(name, cache_pin(spec) or "unpinned")
    return project.path(spec.path) if spec.path else project.path(cfg.build.dependencies_dir) / name


def requirements_met(requires: list[str], options: dict[str, bool]) -> bool:
    """`requires = ["python", "!sanitize"]`: every named option on, every `!`-prefixed one off."""
    for requirement in requires:
        wanted = not requirement.startswith("!")
        if options.get(requirement.removeprefix("!"), False) != wanted:
            return False
    return True


def resolve_all(project: Project, cfg: ForgeConfig) -> list[ResolvedDependency]:
    return [ResolvedDependency(name, spec, dependency_dir(project, cfg, name, spec), project.root) for name, spec in cfg.dependencies.items()]


def required(run: RunContext) -> list[ResolvedDependency]:
    """The dependencies this run's options need; the others are never checked, fetched or given to Premake."""
    return [dep for dep in resolve_all(run.project, run.config) if requirements_met(dep.spec.requires, run.options)]


def missing(run: RunContext) -> list[ResolvedDependency]:
    return [dep for dep in required(run) if not dep.present]


def _premake_entry(dep: ResolvedDependency) -> dict:
    if dep.dir is None:
        cflags, libs = dep.flags() or ([], [])
        return {"kind": "system", "cflags": cflags, "libs": libs, "defines": list(dep.spec.defines)}
    entry = {"kind": str(dep.spec.kind), "dir": dep.dir.as_posix(), "include": dep.include.as_posix(), "defines": list(dep.spec.defines)}
    if dep.sources:
        entry["sources"] = dep.sources.as_posix()
    return entry


def premake_config(run: RunContext) -> dict:
    return {
        "format": PREMAKE_CONFIG_FORMAT,
        "project": run.config.project.name,
        "options": dict(run.options),
        "dependencies": {dep.name: _premake_entry(dep) for dep in required(run)},
    }


def write_premake_config(run: RunContext) -> Path:
    """build/forge/config.json, which premake/forge.lua reads; rewritten only when its content changes."""
    path = run.project.forge_dir / PREMAKE_CONFIG_NAME
    text = json.dumps(premake_config(run), indent=1) + "\n"
    if not path.is_file() or path.read_text(encoding="utf-8") != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    return path


def shown(run: RunContext, path: Path | None) -> str:
    if path is None:
        return "(system)"
    return path.relative_to(run.project.root).as_posix() if path.is_relative_to(run.project.root) else str(path)


def fetch(run: RunContext, deps: list[ResolvedDependency]) -> None:
    from .sources import source_for

    for dep in deps:
        source_for(dep).fetch(run.project.root, dep)


def ensure(run: RunContext, confirm: Callable[[list[ResolvedDependency]], bool] | None = None) -> list[ResolvedDependency]:
    """Make every dependency this run needs present, fetching per [build] fetch; returns what was fetched."""
    absent = missing(run)
    if not absent:
        return []
    for dep in absent:
        if dep.dir is None:
            fetch(run, [dep])
    local = [dep for dep in absent if dep.spec.source == "local"]
    if local:
        fetch(run, local[:1])
    listed = ", ".join(f"{dep.name} ({shown(run, dep.dir)})" for dep in absent)
    if run.fetch == FetchMode.NEVER:
        raise DependencyError(f"Missing dependencies: {listed}. Run: forge deps sync")
    if run.fetch == FetchMode.ASK and not (confirm and confirm(absent)):
        raise DependencyError(f"Missing dependencies: {listed}. Run: forge deps sync, or set [build] fetch = \"auto\"")
    fetch(run, absent)
    return absent
