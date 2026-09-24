import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry, tomledit
from pyforge.config import RunContext, parse_config
from pyforge.config.schema import suggestion

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Executables declared in forge.toml [targets]"
command = registry.make_group(app, group="Target")


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {escape(message)}[/bold red]")
    return typer.Exit(code=1)


def _write(run: RunContext, edit) -> None:
    try:
        tomledit.edit_file(run.project.config_file, edit, validate=parse_config)
    except (tomledit.TomlEditError, ValueError) as error:
        raise _fail(str(error))


@command(name="add", label="Add — declare a [targets] entry")
def add(
    ctx: typer.Context,
    name: str = typer.Argument(..., help="Target name, used as `forge run NAME[:PRESET]`."),
    project: str = typer.Option(..., "--project", help="Premake project name; its path and kind come from the Premake export."),
    default: bool = typer.Option(False, "--default", help="Also set [project] default-target to this target."),
):
    """Add a [targets] entry pointing at an existing Premake project."""
    run: RunContext = ctx.obj
    if name in run.config.targets:
        raise _fail(f"'{name}' is already in forge.toml [targets].")

    if run.dry_run:
        console.print(f"[dim][dry-run] would add target {name} -> {project}[/dim]")
        return

    def edit(text: str) -> str:
        text = tomledit.set_value(text, ["targets", name, "project"], project)
        if default:
            text = tomledit.set_value(text, ["project", "default-target"], name)
        return text

    _write(run, edit)
    console.print(f"[bold green]✓ forge.toml[/bold green] {escape(f'[targets.{name}]')}: project = {escape(tomledit.format_value(project))}")


@command(name="remove", label="Remove — drop a [targets] entry")
def remove(
    ctx: typer.Context,
    name: str = typer.Argument(..., help="Target to remove."),
):
    """Remove a [targets] entry from forge.toml."""
    run: RunContext = ctx.obj
    if name not in run.config.targets:
        raise _fail(f"No target named '{name}' in forge.toml{suggestion(name, run.config.targets)}")

    if run.dry_run:
        console.print(f"[dim][dry-run] would remove target {name}[/dim]")
        return

    def edit(text: str) -> str:
        text = tomledit.unset(text, ["targets", name])
        if run.config.project.default_target == name:
            text = tomledit.unset(text, ["project", "default-target"])
        return text

    _write(run, edit)
    console.print(f"[bold green]✓ Removed {name}[/bold green]")


@command(name="list", label="List — show [targets] entries")
def list_(ctx: typer.Context):
    """Show every [targets] entry, its Premake project, presets, and whether it's the default."""
    from rich.table import Table

    run: RunContext = ctx.obj
    table = Table("name", "project", "presets", "default")
    for name, target in run.config.targets.items():
        is_default = "✓" if name == run.config.project.default_target else ""
        table.add_row(name, target.project, ", ".join(target.presets) or "(none)", is_default)
    console.print(table)
