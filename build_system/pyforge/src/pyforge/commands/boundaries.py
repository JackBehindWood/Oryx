import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry
from pyforge.boundaries import Boundaries
from pyforge.config import RunContext

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Check the layering rules declared in [boundaries]"
command = registry.make_group(app, group="Boundaries")


def _boundaries(run: RunContext) -> Boundaries | None:
    if run.config.boundaries is None:
        console.print("[yellow]No [boundaries] table in forge.toml; nothing to check.[/yellow]")
        return None
    return Boundaries(run.config.boundaries)


@command(name="check", label="Check — enforce the declared layering rules")
def check(
    ctx: typer.Context,
    artifacts: bool = typer.Option(False, "--artifacts", help="Also scan built artifacts for forbidden symbols."),
):
    """Exit non-zero if any include (and with --artifacts, any built symbol) breaks a boundary."""
    run: RunContext = ctx.obj
    boundaries = _boundaries(run)
    if boundaries is None:
        return
    root = run.project.root
    problems = boundaries.check(root)
    if artifacts:
        problems += boundaries.check_artifacts(root, run.options)
    for problem in problems:
        console.print(f"[bold red]boundary violation:[/bold red] {escape(str(problem))}")
    if problems:
        raise typer.Exit(code=1)
    console.print(f"[bold green]✓ {len(boundaries.rules)} boundary rules hold[/bold green]")


@command(name="list", label="List — show the declared rules")
def list_rules(ctx: typer.Context):
    """Print every rule in the order it is applied, then any selector that matches nothing."""
    boundaries = _boundaries(ctx.obj)
    if boundaries is None:
        return
    for rule in {rule.text: rule for rule in boundaries.rules}.values():
        console.print(f"{escape(rule.text)}  [dim]{escape(rule.why)}[/dim]")
    for note in boundaries.unmatched(ctx.obj.project.root):
        console.print(f"[yellow]note: selector {escape(note)}[/yellow]")


@command(name="explain", label="Explain — which rules apply to a file")
def explain(ctx: typer.Context, file: str = typer.Argument(..., help="Path relative to the scanned base, e.g. Core/Window.cpp")):
    """List the rules whose source side matches FILE."""
    boundaries = _boundaries(ctx.obj)
    if boundaries is None:
        return
    applicable = [rule for rule in boundaries.rules if rule.source.matches(file)]
    for rule in applicable:
        console.print(escape(rule.text))
    if not applicable:
        console.print(f"[dim]No rule applies to {escape(file)}[/dim]")
