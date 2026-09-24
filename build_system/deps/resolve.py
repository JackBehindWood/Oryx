"""forge.toml [dependencies] → resolved paths, requirement filtering, and build/forge/config.json for Premake."""

import json
from dataclasses import dataclass
from pathlib import Path

from ..config import Dependency, ForgeConfig, RunContext
from ..project import Project

PREMAKE_CONFIG_NAME = "config.json"
PREMAKE_CONFIG_FORMAT = 1


class DependencyError(RuntimeError):
    pass


@dataclass(frozen=True)
class ResolvedDependency:
    name: str
    spec: Dependency
    dir: Path

    @property
    def include(self) -> Path:
        return self.dir / self.spec.include if self.spec.include else self.dir

    @property
    def sources(self) -> Path | None:
        return self.dir / self.spec.sources if self.spec.sources else None

    @property
    def present(self) -> bool:
        return self.dir.is_dir() and any(self.dir.iterdir())


def dependency_dir(project: Project, cfg: ForgeConfig, name: str, spec: Dependency) -> Path:
    return project.path(spec.path) if spec.path else project.path(cfg.build.dependencies_dir) / name


def requirements_met(spec: Dependency, options: dict[str, bool]) -> bool:
    """`requires = ["python", "!sanitize"]`: every named option on, every `!`-prefixed one off."""
    for requirement in spec.requires:
        wanted = not requirement.startswith("!")
        if options.get(requirement.removeprefix("!"), False) != wanted:
            return False
    return True


def resolve_all(project: Project, cfg: ForgeConfig) -> list[ResolvedDependency]:
    return [ResolvedDependency(name, spec, dependency_dir(project, cfg, name, spec)) for name, spec in cfg.dependencies.items()]


def required(run: RunContext) -> list[ResolvedDependency]:
    """The dependencies this run's options need; the others are never checked, fetched or given to Premake."""
    return [dep for dep in resolve_all(run.project, run.config) if requirements_met(dep.spec, run.options)]


def missing(run: RunContext) -> list[ResolvedDependency]:
    return [dep for dep in required(run) if not dep.present]


def _premake_entry(dep: ResolvedDependency) -> dict:
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


def ensure(run: RunContext) -> None:
    """Fail with guidance when a required dependency is missing on disk."""
    absent = missing(run)
    if absent:
        names = ", ".join(f"{dep.name} ({dep.dir.relative_to(run.project.root) if dep.dir.is_relative_to(run.project.root) else dep.dir})" for dep in absent)
        raise DependencyError(f"Missing dependencies: {names}. Run: git submodule update --init --recursive")
