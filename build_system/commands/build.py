import os
import subprocess
import sys
from pathlib import Path
from typing import Optional

import typer
from rich.console import Console
from rich.markup import escape

from build_system import freshness, options, registry, workspace
from build_system.compile_commands import generate_compile_commands
from build_system.config import ForgeConfig, RunContext, Target
from build_system.config.schema import suggestion
from build_system.setup.generators import build_compile_command
from build_system.setup.premake import ensure_premake, get_premake_executable
from build_system.setup.python_env import PythonEnvError, premake_python_options, python_build_info, write_python_config
from build_system.setup.python_extension import install_extension_pth, remove_extension_pth
from build_system.setup.stale_objects import prune_stale_object_dirs, wipe_outputs_if_options_changed
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


def premake_args(run: RunContext) -> list[str]:
    """Every Premake flag this run's options produce, in a stable order (their hash decides an output wipe)."""
    try:
        python_args = premake_python_options(run.options.get("python", False))
    except PythonEnvError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)
    return options.premake_flags(run.config.options, run.options, run.defines) + python_args


@command(name="configure", label="Configure — generate Premake build files")
def configure(ctx: typer.Context):
    """Generate build files using Premake5."""
    run: RunContext = ctx.obj
    cfg = run.config
    project = run.project

    generator = cfg.premake.generator
    premake_options = premake_args(run)

    if run.dry_run:
        premake = get_premake_executable(project.premake_bin_dir)
        command_line = [str(premake), generator, *premake_options, "--forge-export"]
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    premake = ensure_premake(project.premake_bin_dir, cfg.premake.version)
    if not premake:
        raise typer.Exit(code=1)

    if run.options.get("python", False):
        write_python_config(python_build_info(), project.build_dir)

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

    options_hash = options.options_hash(premake_options)
    previous_hash = previous.options_hash if previous else None
    wiped = wipe_outputs_if_options_changed(project.build_dir, previous_hash, options_hash)
    if wiped and previous_hash:
        console.print("[yellow]⚠️ Build options changed; cleared previous binaries and objects.[/yellow]\n")

    ws = require_workspace(run)
    (project.build_dir / ".sources").unlink(missing_ok=True)
    if not wiped:
        _clear_targets_that_lost_sources(previous, ws)
        for name in prune_stale_object_dirs(ws, run.profile, project.build_dir):
            console.print(
                f"[yellow]⚠️ Cleared stale object cache for {name} "
                "(moved/renamed/deleted source detected).[/yellow]\n"
            )
    freshness.record(project, ws, options_hash)

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

    reason = None if run.dry_run else freshness.stale(run.project, options.options_hash(premake_args(run)))
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


EXECUTABLE_KINDS = ("ConsoleApp", "WindowedApp")


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {escape(message)}[/bold red]")
    return typer.Exit(code=1)


def resolve_target(cfg: ForgeConfig, spec: str | None) -> tuple[str, Target, list[str]]:
    """TARGET[:PRESET] → (name, target, preset arguments); no spec means [project] default-target."""
    name, _, preset = (spec or cfg.project.default_target).partition(":")
    if not name:
        raise _fail("No target given and no [project] default-target in forge.toml.")
    if name not in cfg.targets:
        available = ", ".join(cfg.targets) or "(none in forge.toml)"
        raise _fail(f"Unknown target '{name}'{suggestion(name, cfg.targets)} (available: {available})")
    target = cfg.targets[name]
    if not preset:
        return name, target, []
    if preset not in target.presets:
        available = ", ".join(target.presets) or "(none)"
        raise _fail(f"Unknown preset '{name}:{preset}'{suggestion(preset, target.presets)} (available: {available})")
    return name, target, list(target.presets[preset])


def _can_replace_process() -> bool:
    return os.name == "posix"


def _exec(argv: list[str], cwd: Path, replace_process: bool) -> None:
    sys.stdout.flush()
    sys.stderr.flush()
    if replace_process and _can_replace_process():
        os.chdir(cwd)
        os.execv(argv[0], argv)
        return
    code = subprocess.run(argv, cwd=cwd).returncode
    if code:
        raise typer.Exit(code=code)


@command(
    name="run",
    label="Run — launch a [targets] executable",
    context_settings={"allow_extra_args": True, "ignore_unknown_options": True},
)
def run_project(
    ctx: typer.Context,
    target: Optional[str] = typer.Argument(
        None,
        metavar="[TARGET[:PRESET]]",
        help="A forge.toml [targets] entry, optionally with one of its presets (default: [project] default-target). "
        "Arguments after `--` go to the program.",
    ),
):
    """Run a compiled [targets] executable from the project root, replacing forge's process."""
    run: RunContext = ctx.obj
    extra = list(ctx.args)
    if target and target.startswith("-"):
        target, extra = None, [target, *extra]

    name, entry, preset_args = resolve_target(run.config, target)
    config = require_workspace(run).config(entry.project, run.profile)
    if config.kind not in EXECUTABLE_KINDS:
        raise _fail(f"Target '{name}' is Premake project '{entry.project}', a {config.kind}, not an executable.")
    argv = [str(config.target), *preset_args, *extra]

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(argv)}[/dim]")
        return

    if not config.target.exists():
        console.print(f"[bold red]✗ Executable missing at:[/bold red] {config.target}")
        console.print("  [dim]Run 'forge build compile' first.[/dim]")
        raise typer.Exit(code=1)

    console.print(f"[bold blue]🚀 Running {name} ({run.profile})...[/bold blue]")
    _exec(argv, run.project.root, replace_process=not run.interactive)
