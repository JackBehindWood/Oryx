import subprocess
from typing import Optional

import typer
from rich.console import Console

from build_system import registry
from build_system.compile_commands import generate_compile_commands
from build_system.config import BUILD_DIR, PROJECT_ROOT, RunContext
from build_system.setup.generators import build_compile_command
from build_system.setup.premake import ensure_premake, get_premake_executable
from build_system.setup.python_env import PythonEnvError, premake_python_options, python_build_info, write_python_config
from build_system.setup.python_extension import install_extension_pth
from build_system.setup.stale_objects import (
    clear_outputs_if_python_changed,
    clear_outputs_of_removed_sources,
    prune_stale_object_dirs,
    record_source_manifest,
    sources_changed,
)
from build_system.utils import remove_directory, run_command

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Configure, build, and clean the engine binaries"
command = registry.make_group(app, group="Build")


@command(name="configure", label="Configure — generate Premake build files")
def configure(ctx: typer.Context):
    """Generate build files using Premake5."""
    run: RunContext = ctx.obj
    cfg = run.config

    try:
        premake_options = premake_python_options(cfg)
        python_info = python_build_info() if cfg.python_enabled else None
    except PythonEnvError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)

    if cfg.sanitize:
        premake_options = [*premake_options, "--sanitize"]

    if run.dry_run:
        premake = get_premake_executable()
        command_line = [str(premake), cfg.build_generator, *premake_options]
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    premake = ensure_premake()
    if not premake:
        raise typer.Exit(code=1)

    if python_info is not None:
        write_python_config(python_info)

    command_line = [str(premake), cfg.build_generator, *premake_options]

    try:
        with console.status("[bold blue]⚙️ Configuring build...[/bold blue]"):
            result = run_command(command_line, cwd=PROJECT_ROOT)
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print("[bold green]✓ Build files generated successfully.[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Failed to configure build:[/bold red]\n{error.stderr}")
        raise typer.Exit(code=1)

    if clear_outputs_if_python_changed(premake_options):
        console.print("[yellow]⚠️ Build options changed (Python or --sanitize); cleared previous binaries and objects.[/yellow]\n")

    for project in clear_outputs_of_removed_sources():
        console.print(f"[yellow]⚠️ Cleared {project} binaries (a source file was removed).[/yellow]\n")
    record_source_manifest()

    for project in prune_stale_object_dirs(cfg):
        console.print(
            f"[yellow]⚠️ Cleared stale object cache for {project} "
            "(moved/renamed/deleted source detected).[/yellow]\n"
        )

    try:
        if generate_compile_commands(cfg):
            console.print("[bold green]✓ compile_commands.json generated.[/bold green]\n")
    except Exception as error:
        console.print(f"[yellow]⚠️ Could not generate compile_commands.json: {error}[/yellow]\n")


@command(name="compile", label="Compile — build engine binaries", requires_vendor=True)
def compile_project(ctx: typer.Context):
    """Compile the engine binaries for the targeted configuration."""
    run: RunContext = ctx.obj
    cfg = run.config

    if not run.dry_run and sources_changed():
        console.print("[yellow]⚠️ Source files were added or removed; regenerating build files first.[/yellow]\n")
        ctx.invoke(configure, ctx)

    try:
        command_line = build_compile_command(cfg, BUILD_DIR)
    except ValueError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    try:
        with console.status(f"[bold blue]🔨 Building project ({cfg.profile})...[/bold blue]"):
            result = run_command(command_line)
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print(f"[bold green]✓ Build successful ({cfg.profile})[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Build failed:[/bold red]\n{error.stderr}")
        raise typer.Exit(code=1)

    if cfg.python_enabled:
        install_extension_pth(cfg.binary_path / "OryxPython")


@command(name="clean", label="Clean — remove build artifacts")
def clean(ctx: typer.Context):
    """Remove generated build artifacts and binaries."""
    run: RunContext = ctx.obj

    if run.dry_run:
        console.print(f"[dim][dry-run] would remove: {BUILD_DIR}[/dim]")
        return

    console.print("[bold yellow]🧹 Cleaning build artifacts...[/bold yellow]")
    if BUILD_DIR.exists():
        remove_directory(BUILD_DIR)
        console.print(f"[green]✓ Removed directory:[/green] {BUILD_DIR}")


@command(name="all", label="All — configure, compile, and test")
def run_all(ctx: typer.Context):
    """Configure, compile, and execute tests sequentially."""
    ctx.invoke(configure, ctx)
    ctx.invoke(compile_project, ctx)
    from build_system.commands.test import run_tests
    ctx.invoke(run_tests, ctx)


@command(name="run", label="Run — launch the Oasis sandbox executable")
def run_project(
    ctx: typer.Context,
    simulate: Optional[str] = typer.Option(
        None,
        "--simulate",
        help="Run a headless <strategyA>,<strategyB>,<matchCount> batch instead of the interactive prompt.",
    ),
    benchmark: bool = typer.Option(
        False,
        "--benchmark",
        help="With --simulate: print a timing/throughput/Instrumentation report on completion.",
    ),
    game: Optional[str] = typer.Option(
        None,
        "--game",
        help="Registered game id to play (built in or script-defined); prompts when omitted.",
    ),
    opponent: Optional[str] = typer.Option(
        None,
        "--opponent",
        help="Opponent strategy id, or 'human'; prompts when omitted.",
    ),
):
    """Run the compiled Oasis sandbox executable."""
    run: RunContext = ctx.obj
    cfg = run.config

    exe_path = cfg.executable_path("oasis")

    args = [str(exe_path)]
    if game:
        args.append(f"--game={game}")
    if opponent:
        args.append(f"--opponent={opponent}")
    if simulate:
        args.append(f"--simulate={simulate}")
    if benchmark:
        args.append("--benchmark")

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(args)}[/dim]")
        return

    console.print(f"[bold blue]🚀 Running Oasis ({cfg.profile})...[/bold blue]")

    if not exe_path.exists():
        console.print(f"[bold red]✗ Executable missing at:[/bold red] {exe_path}")
        console.print("  [dim]Run 'build build compile' first.[/dim]")
        raise typer.Exit(code=1)

    try:
        run_command(args, capture_output=False)
        console.print("\n[bold green]✓ Oasis exited successfully[/bold green]\n")
    except subprocess.CalledProcessError:
        console.print("[bold red]✗ Oasis exited with an error.[/bold red]")
        raise typer.Exit(code=1)
