import json
import os
from typing import Literal

import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry
from pyforge.boundaries import Boundaries, BoundarySpec, Violation, specs_of
from pyforge.config import RunContext

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Check the layering rules declared by a plugin or in [boundaries]"
command = registry.make_group(app, group="Boundaries")

Format = Literal["text", "json", "github"]


def _specs(run: RunContext) -> list[BoundarySpec]:
    specs = [spec for result in run.pm.hook.forge_boundaries(ctx=run) for spec in specs_of(result)]
    if run.config.boundaries is not None:
        specs.append(BoundarySpec.from_table(run.config.boundaries))
    names = [spec.name for spec in specs]
    if len(set(names)) != len(names):
        console.print(f"[bold red]✗ Two boundary specs share a name ({', '.join(sorted(n for n in set(names) if names.count(n) > 1))}); give one a distinct name=.[/bold red]")
        raise typer.Exit(code=1)
    return specs


def _boundaries(run: RunContext) -> list[Boundaries]:
    specs = _specs(run)
    if not specs:
        console.print("[yellow]No boundaries declared (no plugin spec, no [boundaries] table); nothing to check.[/yellow]")
    return [Boundaries(spec) for spec in specs]


def _default_format() -> Format:
    return "github" if os.environ.get("GITHUB_ACTIONS") == "true" else "text"


def _emit(problems: list[Violation], notes: list[Violation], fmt: Format, several: bool) -> None:
    if fmt == "json":
        rows = [{"spec": p.spec, "rule": p.rule, "file": p.path or p.subject, "line": p.line, "message": p.message, "waived": p in notes} for p in [*problems, *notes]]
        print(json.dumps(rows, indent=2))
        return
    for problem in problems:
        prefix = f"[{problem.spec}] " if several and problem.spec else ""
        console.print(f"[bold red]boundary violation:[/bold red] {escape(prefix + problem.location)}: {escape(problem.message)}")
        if fmt == "github" and problem.path:
            print(f"::error file={problem.path},line={problem.line or 1},title=Boundary violation::{problem.message}")
    for note in notes:
        console.print(f"[dim]waived: {escape(note.location)}: {escape(note.message)}[/dim]")


@command(name="check", label="Check — enforce the declared layering rules")
def check(
    ctx: typer.Context,
    artifacts: bool = typer.Option(False, "--artifacts", help="Also scan built artifacts for forbidden symbols."),
    fmt: str = typer.Option("", "--format", help="text, json or github (default: github inside GitHub Actions, otherwise text)."),
):
    """Exit non-zero if any include (and with --artifacts, any built symbol) breaks a boundary."""
    run: RunContext = ctx.obj
    chosen = fmt or _default_format()
    if chosen not in ("text", "json", "github"):
        console.print(f"[bold red]✗ --format must be text, json or github, not {chosen!r}[/bold red]")
        raise typer.Exit(code=1)
    all_boundaries = _boundaries(run)
    if not all_boundaries:
        return
    root = run.project.root
    problems: list[Violation] = []
    notes: list[Violation] = []
    for boundaries in all_boundaries:
        problems += boundaries.dead_parts(root) + boundaries.check(root) + boundaries.stale_waivers(root)
        notes += boundaries.waived
        if artifacts:
            problems += boundaries.check_artifacts(root, run.options)
    _emit(problems, notes, chosen, len(all_boundaries) > 1)  # type: ignore[arg-type]
    if problems:
        raise typer.Exit(code=1)
    if chosen != "json":
        rules = sum(len(boundaries.rules) for boundaries in all_boundaries)
        console.print(f"[bold green]✓ {rules} boundary rules hold[/bold green]")


@command(name="list", label="List — show the declared rules")
def list_rules(ctx: typer.Context):
    """Print every rule in the order it is applied, then any selector that matches nothing."""
    several = len(_boundaries_cache := _boundaries(ctx.obj)) > 1
    for boundaries in _boundaries_cache:
        if several:
            console.print(f"[bold]{escape(boundaries.spec.name)}[/bold]")
        for rule in {rule.text: rule for rule in boundaries.rules}.values():
            declared = f"  [dim]({escape(rule.declared)})[/dim]" if rule.declared else ""
            reason = f"  [dim]{escape(rule.because)}[/dim]" if rule.because else ""
            console.print(f"{escape(rule.text)}{reason}{declared}")
        for optional in boundaries.spec.optionals:
            console.print(f"[cyan]optional[/cyan] {escape(optional.option)}: {escape(optional.selection.label)}  [dim]{escape(optional.because)}[/dim]")
        for waiver in boundaries.spec.waivers:
            console.print(f"[yellow]waiver[/yellow] {escape(waiver.label)}  [dim]until {waiver.expires}: {escape(waiver.because)}[/dim]")
        for note in boundaries.unmatched(ctx.obj.project.root):
            console.print(f"[yellow]note: selector {escape(note)}[/yellow]")


@command(name="explain", label="Explain — which rules apply to a file")
def explain(ctx: typer.Context, file: str = typer.Argument(..., help="Path relative to the scanned base, e.g. Core/Window.cpp")):
    """List the rules whose source side matches FILE."""
    found = False
    for boundaries in _boundaries(ctx.obj):
        for rule in (rule for rule in boundaries.rules if rule.source.matches(file)):
            console.print(escape(rule.text))
            found = True
    if not found:
        console.print(f"[dim]No rule applies to {escape(file)}[/dim]")
