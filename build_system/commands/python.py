import subprocess

import typer
from rich.console import Console

from build_system import registry
from build_system.config import PROJECT_ROOT, RunContext
from build_system.utils import run_command

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Python extension tooling (stub generation)"
command = registry.make_group(app, group="Python")


@command(name="stubs", label="Stubs — regenerate the oryx extension's .pyi files")
def generate_stubs(ctx: typer.Context):
    """Regenerates Oryx/backends/Python/stubs/oryx/*.pyi from the built extension."""
    run: RunContext = ctx.obj
    command_line = ["uv", "run", "--group", "stubs", "python", "-m", "pybind11_stubgen", "oryx", "-o", "Oryx/backends/Python/stubs"]

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    try:
        with console.status("[bold blue]🐍 Regenerating stubs...[/bold blue]"):
            result = run_command(command_line, cwd=PROJECT_ROOT)
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print("[bold green]✓ Stubs regenerated.[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Failed to regenerate stubs:[/bold red]\n{error.stderr or error.stdout}")
        raise typer.Exit(code=1)
