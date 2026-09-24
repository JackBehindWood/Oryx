from pathlib import Path
from typing import Literal, Optional

import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry, tomledit
from pyforge.config import RunContext, parse_config
from pyforge.deps.detect import detect_layout
from pyforge.deps.resolve import (
    DependencyError,
    ResolvedDependency,
    ensure,
    fetch,
    requirements_met,
    resolve_all,
    shown,
)

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Third-party dependencies declared in forge.toml [dependencies]"
command = registry.make_group(app, group="Deps")


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {escape(message)}[/bold red]")
    return typer.Exit(code=1)


def _confirm_fetch(deps: list[ResolvedDependency]) -> bool:
    from pyforge.interactive import is_interactive

    if not is_interactive():
        return False
    import questionary

    return bool(questionary.confirm(f"Fetch missing dependencies ({', '.join(dep.name for dep in deps)})?", default=True).ask())


def ensure_or_exit(run: RunContext) -> None:
    try:
        for dep in ensure(run, confirm=_confirm_fetch):
            console.print(f"[green]✓ Fetched {dep.name}[/green] into {shown(run, dep.dir)}")
    except DependencyError as error:
        raise _fail(str(error))


def _find(run: RunContext, name: str) -> ResolvedDependency:
    for dep in resolve_all(run.project, run.config):
        if dep.name == name:
            return dep
    from pyforge.config.schema import suggestion

    raise _fail(f"No dependency named '{name}' in forge.toml{suggestion(name, run.config.dependencies)}")


def _write(run: RunContext, edit) -> None:
    try:
        tomledit.edit_file(run.project.config_file, edit, validate=parse_config)
    except (tomledit.TomlEditError, ValueError) as error:
        raise _fail(str(error))


def _entry(source: str, path: str, default_dir: str, layout: dict[str, str], defines: list[str], requires: list[str]) -> dict:
    entry: dict = {}
    if source != "submodule":
        entry["source"] = source
    if path and path != default_dir:
        entry["path"] = path
    if layout["kind"] != "header":
        entry["kind"] = layout["kind"]
    for key in ("include", "sources"):
        if layout[key]:
            entry[key] = layout[key]
    if defines:
        entry["defines"] = defines
    if requires:
        entry["requires"] = requires
    return entry


def _consumer_snippet(name: str, kind: str) -> str:
    lines = [f'includedirs {{ forge.include("{name}") }}']
    if kind == "static":
        lines.append(f'links {{ "{name}" }}')
    return "\n".join(f"    {line}" for line in lines)


@command(name="add", label="Add — declare a dependency (local folder or git submodule)")
def add(
    ctx: typer.Context,
    name: str = typer.Argument(..., help="Dependency name; also its folder name under [build] dependencies-dir."),
    local: bool = typer.Option(False, "--local", help="Files you put in place yourself (never fetched or deleted by forge)."),
    submodule: Optional[str] = typer.Option(None, "--submodule", metavar="URL", help="Add it as a git submodule from URL."),
    path: Optional[str] = typer.Option(None, "--path", help="Folder relative to the project root (default: <dependencies-dir>/NAME)."),
    kind: Optional[Literal["static", "header"]] = typer.Option(None, "--kind", help="static (compiled from its sources) or header-only; detected when omitted."),
    include: Optional[str] = typer.Option(None, "--include", help="Include folder inside it; detected when omitted."),
    sources: Optional[str] = typer.Option(None, "--sources", help="Source folder inside it, for --kind static; detected when omitted."),
    define: list[str] = typer.Option([], "--define", help="Preprocessor define for its own compilation (repeatable)."),
    requires: list[str] = typer.Option([], "--requires", help="Only needed when this [options] switch is on; prefix ! for off (repeatable)."),
):
    """Add a [dependencies] entry, detecting its layout when the files are present."""
    run: RunContext = ctx.obj
    if local == bool(submodule):
        raise _fail("Pass exactly one of --local or --submodule URL.")
    if name in run.config.dependencies:
        raise _fail(f"'{name}' is already in forge.toml [dependencies].")

    default_dir = Path(run.config.build.dependencies_dir, name).as_posix()
    folder = path or default_dir
    source = "local" if local else "submodule"
    from pyforge.config import Dependency

    dep = ResolvedDependency(name, Dependency(source=source, path=folder), run.project.path(folder))

    if run.dry_run:
        console.print(f"[dim][dry-run] would add {name} ({source}) at {folder}[/dim]")
        return

    if submodule and not dep.present:
        from pyforge.deps.sources.submodule import SubmoduleSource

        try:
            SubmoduleSource().add(run.project.root, dep, submodule)
        except DependencyError as error:
            raise _fail(str(error))
        console.print(f"[bold green]✓ Added submodule {folder}[/bold green]")
    elif local and not dep.present:
        raise _fail(f"Put the files for '{name}' at {folder} first, then run this again.")

    layout = detect_layout(dep.dir, name)
    layout.update({key: value for key, value in (("kind", kind), ("include", include), ("sources", sources)) if value is not None})
    entry = _entry(source, folder, default_dir, layout, define, requires)
    _write(run, lambda text: tomledit.set_value(text, ["dependencies", name], entry))

    console.print(f"[bold green]✓ forge.toml[/bold green] {escape('[dependencies]')}: {escape(tomledit.format_key(name))} = {escape(tomledit.format_value(entry))}")
    console.print(f"  [dim]Use it from a project's premake5.lua:[/dim]\n{escape(_consumer_snippet(name, layout['kind']))}")


@command(name="sync", label="Sync — fetch every missing required dependency")
def sync(ctx: typer.Context):
    """Fetch every dependency this build needs that is missing, whatever [build] fetch says."""
    run: RunContext = ctx.obj
    absent = [dep for dep in resolve_all(run.project, run.config) if requirements_met(dep.spec.requires, run.options) and not dep.present]
    if run.dry_run:
        for dep in absent:
            console.print(f"[dim][dry-run] would fetch {dep.name} into {shown(run, dep.dir)}[/dim]")
        return
    try:
        fetch(run, absent)
    except DependencyError as error:
        raise _fail(str(error))
    for dep in absent:
        console.print(f"[green]✓ Fetched {dep.name}[/green] into {shown(run, dep.dir)}")
    console.print(f"[bold green]✓ {len(absent)} fetched; every required dependency is present.[/bold green]")


@command(name="update", label="Update — move a dependency to a new revision")
def update(
    ctx: typer.Context,
    name: str = typer.Argument(..., help="Dependency to update."),
    rev: Optional[str] = typer.Option(None, "--rev", help="Tag, branch or commit (default: the remote's tracked branch)."),
):
    """Move a submodule dependency to `--rev` (or its remote head); commit the new pin with git."""
    run: RunContext = ctx.obj
    dep = _find(run, name)
    from pyforge.deps.sources import source_for

    if run.dry_run:
        console.print(f"[dim][dry-run] would update {name} to {rev or 'the remote head'}[/dim]")
        return
    try:
        source = source_for(dep)
        source.update(run.project.root, dep, rev)
        pin = source.pin(run.project.root, dep)
    except DependencyError as error:
        raise _fail(str(error))
    console.print(f"[bold green]✓ {name} is now at {pin or rev or 'the remote head'}[/bold green]; commit {shown(run, dep.dir)} to record it.")


def _untracked(run: RunContext, deps: list[ResolvedDependency]) -> list[Path]:
    base = run.project.path(run.config.build.dependencies_dir)
    known = {dep.dir for dep in deps}
    return sorted(p for p in base.iterdir() if p.is_dir() and not p.name.startswith(".") and p not in known) if base.is_dir() else []


@command(name="status", label="Status — list dependencies and whether they are present")
def status(ctx: typer.Context):
    """Show each dependency's source, pin and state, plus folders no entry refers to."""
    from rich.table import Table

    from pyforge.deps.sources import source_for

    run: RunContext = ctx.obj
    deps = resolve_all(run.project, run.config)
    table = Table("name", "source", "kind", "path", "pin", "state", "requires")
    for dep in deps:
        needed = requirements_met(dep.spec.requires, run.options)
        state = ("[green]present[/green]" if dep.present else "[red]missing[/red]") if needed else "[dim]not needed[/dim]"
        table.add_row(dep.name, dep.spec.source, str(dep.spec.kind), shown(run, dep.dir), source_for(dep).pin(run.project.root, dep), state, ", ".join(dep.spec.requires))
    console.print(table)
    for folder in _untracked(run, deps):
        console.print(f"[yellow]untracked:[/yellow] {shown(run, folder)} → forge deps add {folder.name} --local")


@command(name="remove", label="Remove — drop a dependency from forge.toml")
def remove(
    ctx: typer.Context,
    name: str = typer.Argument(..., help="Dependency to remove."),
    yes: bool = typer.Option(False, "--yes", "-y", help="Don't ask for confirmation."),
):
    """Remove a [dependencies] entry; a submodule is also deinitialised and removed from git (local files are kept)."""
    run: RunContext = ctx.obj
    dep = _find(run, name)
    from pyforge.deps.sources import source_for

    action = "deinit and remove the submodule" if dep.spec.source == "submodule" else "keep its files"
    if run.dry_run:
        console.print(f"[dim][dry-run] would remove {name} from forge.toml and {action}[/dim]")
        return
    if not yes and not typer.confirm(f"Remove '{name}' from forge.toml and {action} at {shown(run, dep.dir)}?"):
        raise typer.Exit(code=1)
    try:
        source_for(dep).remove(run.project.root, dep)
    except DependencyError as error:
        raise _fail(str(error))
    _write(run, lambda text: tomledit.unset(text, ["dependencies", name]))
    console.print(f"[bold green]✓ Removed {name}[/bold green]")
