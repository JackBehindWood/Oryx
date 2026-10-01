import subprocess
from pathlib import Path

import typer
from rich.console import Console
from rich.markup import escape

from .. import options
from ..config import RunContext
from ..utils import run_command
from .install import lua_scripts_dir

console = Console()


def premake_args(run: RunContext) -> list[str]:
    """Every Premake flag this run's options produce, in a stable order (their hash decides an output wipe)."""
    try:
        plugin_args = [arg for result in run.pm.hook.forge_premake_args(ctx=run) if result for arg in result]
    except RuntimeError as error:
        console.print(f"[bold red]✗ {escape(str(error))}[/bold red]")
        raise typer.Exit(code=1)
    return options.premake_flags(run.config.options, run.options, run.defines) + plugin_args


def scripts_flag() -> str:
    """`--scripts=<pyforge's lua dir>`, so a project's premake5.lua can `require "forge"` regardless
    of where pyforge is installed, instead of an `include` with a hardcoded relative path."""
    return f"--scripts={lua_scripts_dir()}"


def export_command(executable: Path, generator: str, premake_options: list[str]) -> list[str]:
    return [str(executable), generator, scripts_flag(), *premake_options, "--forge-export"]


def run_export(executable: Path, generator: str, premake_options: list[str], cwd: Path, verbose: bool = False) -> None:
    try:
        with console.status("[bold blue]⚙️ Configuring build...[/bold blue]"):
            result = run_command(export_command(executable, generator, premake_options), cwd=cwd)
        if verbose and result.stdout:
            console.print(result.stdout)
        console.print("[bold green]✓ Build files generated successfully.[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Failed to configure build:[/bold red]\n{error.stderr}")
        raise typer.Exit(code=1)
