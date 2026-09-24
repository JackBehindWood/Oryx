import subprocess
import sys

import typer
from rich.console import Console
from rich.markup import escape

from build_system import registry
from build_system.config import RunContext
from build_system.utils import missing_module_hint, run_command

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Python extension tooling (stub generation)"
command = registry.make_group(app, group="Python")

EXTENSION_MODULE = "oryx"


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {escape(message)}[/bold red]")
    return typer.Exit(code=1)


@command(name="stubs", label="Stubs — regenerate the oryx extension's .pyi files")
def generate_stubs(ctx: typer.Context):
    """Regenerate the extension's .pyi files into forge.toml's [tool.oryx] stubs-dir."""
    run: RunContext = ctx.obj
    stubs_dir = run.config.tool.get(EXTENSION_MODULE, {}).get("stubs-dir")
    if not stubs_dir:
        raise _fail(f"Set [tool.{EXTENSION_MODULE}] stubs-dir in {run.project.config_file.name} to regenerate stubs.")
    command_line = [sys.executable, "-m", "pybind11_stubgen", EXTENSION_MODULE, "-o", stubs_dir]

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    hint = missing_module_hint("pybind11_stubgen", "stubs", ["pybind11-stubgen"])
    if hint:
        raise _fail(hint)
    try:
        with console.status("[bold blue]🐍 Regenerating stubs...[/bold blue]"):
            result = run_command(command_line, cwd=run.project.root)
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print("[bold green]✓ Stubs regenerated.[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Failed to regenerate stubs:[/bold red]\n{error.stderr or error.stdout}")
        raise typer.Exit(code=1)
