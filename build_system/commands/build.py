import subprocess
from typing import Optional

import typer
from rich.console import Console

from build_system import freshness, registry, workspace
from build_system.compile_commands import generate_compile_commands
from build_system.config import RunContext
from build_system.setup.generators import build_compile_command
from build_system.setup.premake import ensure_premake, get_premake_executable
from build_system.setup.python_env import PythonEnvError, premake_python_options, python_build_info, write_python_config
from build_system.setup.python_extension import install_extension_pth, remove_extension_pth
from build_system.setup.stale_objects import clear_outputs_if_python_changed, prune_stale_object_dirs
from build_system.utils import remove_directory, run_command
from build_system.workspace import Workspace, WorkspaceError

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Configure, build, and clean the engine binaries"
command = registry.make_group(app, group="Build")


def require_workspace(run: RunContext) -> Workspace:
    try:
        return workspace.require(run.project)
    except WorkspaceError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)


def _clear_targets_that_lost_sources(previous: freshness.Stamp | None, ws: Workspace) -> None:
    # `ar` keeps a removed source's archive member and Make won't relink an executable that only lost an object.
    for name in freshness.projects_that_lost_sources(previous, ws):
        ws_project = ws.projects.get(name)
        for cfg in ws_project.configs if ws_project else ():
            cfg.target.unlink(missing_ok=True)
        console.print(f"[yellow]⚠️ Cleared {name} binaries (a source file was removed).[/yellow]\n")


@command(name="configure", label="Configure — generate Premake build files")
def configure(ctx: typer.Context):
    """Generate build files using Premake5."""
    run: RunContext = ctx.obj
    cfg = run.config
    project = run.project

    python_enabled = run.options.get("python", False)
    generator = cfg.premake.generator
    try:
        premake_options = premake_python_options(python_enabled)
        python_info = python_build_info() if python_enabled else None
    except PythonEnvError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)

    if run.options.get("sanitize", False):
        premake_options = [*premake_options, "--sanitize"]

    if run.dry_run:
        premake = get_premake_executable(project.premake_bin_dir)
        command_line = [str(premake), generator, *premake_options, "--forge-export"]
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    premake = ensure_premake(project.premake_bin_dir, cfg.premake.version)
    if not premake:
        raise typer.Exit(code=1)

    if python_info is not None:
        write_python_config(python_info, project.build_dir)

    command_line = [str(premake), generator, *premake_options, "--forge-export"]
    previous = freshness.load_stamp(project)

    try:
        with console.status("[bold blue]⚙️ Configuring build...[/bold blue]"):
            result = run_command(command_line, cwd=project.root)
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print("[bold green]✓ Build files generated successfully.[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Failed to configure build:[/bold red]\n{error.stderr}")
        raise typer.Exit(code=1)

    if clear_outputs_if_python_changed(premake_options, project.build_dir):
        console.print("[yellow]⚠️ Build options changed (Python or --sanitize); cleared previous binaries and objects.[/yellow]\n")

    ws = require_workspace(run)
    (project.build_dir / ".sources").unlink(missing_ok=True)
    _clear_targets_that_lost_sources(previous, ws)
    freshness.record(project, ws)

    for name in prune_stale_object_dirs(ws, run.profile, project.build_dir):
        console.print(
            f"[yellow]⚠️ Cleared stale object cache for {name} "
            "(moved/renamed/deleted source detected).[/yellow]\n"
        )

    try:
        if generate_compile_commands(ws.token(run.profile), project.build_dir):
            console.print("[bold green]✓ compile_commands.json generated.[/bold green]\n")
    except Exception as error:
        console.print(f"[yellow]⚠️ Could not generate compile_commands.json: {error}[/yellow]\n")


@command(name="compile", label="Compile — build engine binaries", requires_vendor=True)
def compile_project(ctx: typer.Context):
    """Compile the engine binaries for the targeted configuration."""
    run: RunContext = ctx.obj
    cfg = run.config

    reason = None if run.dry_run else freshness.stale(run.project)
    if reason:
        console.print(f"[yellow]⚠️ Regenerating build files first ({reason}).[/yellow]\n")
        ctx.invoke(configure, ctx)

    ws = workspace.load(run.project)
    token = ws.token(run.profile) if ws else "<from export>"
    try:
        command_line = build_compile_command(cfg.premake.generator, token, run.project.build_dir, run.jobs)
    except ValueError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    try:
        with console.status(f"[bold blue]🔨 Building project ({run.profile})...[/bold blue]"):
            result = run_command(command_line)
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print(f"[bold green]✓ Build successful ({run.profile})[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Build failed:[/bold red]\n{error.stderr}")
        raise typer.Exit(code=1)

    python_enabled = run.options.get("python", False)
    if python_enabled and not run.options.get("sanitize", False):
        install_extension_pth(ws.target_path("OryxPython", run.profile).parent)
    elif remove_extension_pth():
        # A sanitized oryx.so needs the ASan runtime preloaded, which a plain `python` never has.
        reason = "a --sanitize extension can't be imported by plain python" if python_enabled else "this build has no Python extension"
        console.print(f"[dim]Removed the venv's `import oryx` path: {reason}; a normal build restores it.[/dim]\n")


@command(name="clean", label="Clean — remove build artifacts")
def clean(ctx: typer.Context):
    """Remove generated build artifacts and binaries."""
    run: RunContext = ctx.obj
    build_dir = run.project.build_dir

    if run.dry_run:
        console.print(f"[dim][dry-run] would remove: {build_dir}[/dim]")
        return

    console.print("[bold yellow]🧹 Cleaning build artifacts...[/bold yellow]")
    if build_dir.exists():
        remove_directory(build_dir)
        console.print(f"[green]✓ Removed directory:[/green] {build_dir}")
    if remove_extension_pth():
        console.print("[green]✓ Removed the venv's `import oryx` path[/green]")


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

    target = cfg.targets[cfg.project.default_target]
    exe_path = require_workspace(run).target_path(target.project, run.profile)

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

    console.print(f"[bold blue]🚀 Running {target.project} ({run.profile})...[/bold blue]")

    if not exe_path.exists():
        console.print(f"[bold red]✗ Executable missing at:[/bold red] {exe_path}")
        console.print("  [dim]Run 'forge build compile' first.[/dim]")
        raise typer.Exit(code=1)

    try:
        run_command(args, cwd=run.project.root, capture_output=False)
        console.print(f"\n[bold green]✓ {target.project} exited successfully[/bold green]\n")
    except subprocess.CalledProcessError:
        console.print(f"[bold red]✗ {target.project} exited with an error.[/bold red]")
        raise typer.Exit(code=1)
