from pathlib import Path
from typing import Optional

import typer
from rich.console import Console

from build_system import registry
from build_system.config import BuildConfig, RunContext
from build_system.project import Project, ProjectNotFound

console = Console()

app = typer.Typer(
    name="forge",
    help="Oryx Engine Build & Tooling CLI",
    invoke_without_command=True,
    rich_markup_mode="markdown",
)

# Every build_system/commands/<name>.py module is auto-discovered and
# attached here — dropping a new module in that directory (with its own
# `app`, `GROUP_HELP`, and `@registry.command(...)`-decorated functions)
# requires no edits to this file. See build_system/registry.py.
for _module in registry.discover_command_modules():
    _name = _module.__name__.rsplit(".", 1)[-1]
    app.add_typer(_module.app, name=_name, help=getattr(_module, "GROUP_HELP", ""))

def _discover(ctx: typer.Context) -> Project:
    try:
        return Project.discover()
    except ProjectNotFound:
        if ctx.invoked_subcommand != "config":
            raise
        return Project.from_config(Path.cwd() / "forge.toml")


@app.callback()
def main(
    ctx: typer.Context,
    config_path: Optional[Path] = typer.Option(
        None,
        "--config",
        "-c",
        help="Path to the project's config file (default: the nearest forge.toml at or above the current directory).",
    ),
    profile: str = typer.Option(
        "debug",
        "--profile",
        "-p",
        help="Build configuration profile: [debug | release | dist]",
    ),
    verbose: bool = typer.Option(
        False,
        "--verbose",
        "-v",
        help="Show full command output and the underlying commands being run.",
    ),
    dry_run: bool = typer.Option(
        False,
        "--dry-run",
        help="Print the commands that would run without executing them.",
    ),
    no_python: bool = typer.Option(
        False,
        "--no-python",
        help="Build without the Python scripting backend (overrides [python] enabled in oryx.toml).",
    ),
    sanitize: bool = typer.Option(
        False,
        "--sanitize",
        help="Build with AddressSanitizer + UndefinedBehaviorSanitizer, keeping the profile's optimize "
        "level and forcing debug symbols on (a dev/CI diagnostic tool, not a build you'd ship).",
    ),
):
    """Global context setup executed before running commands."""
    try:
        project = Project.from_config(config_path) if config_path else _discover(ctx)
        cfg = BuildConfig.load(project.config_file)
        cfg.profile = profile
        if no_python:
            cfg.python_enabled = False
        if sanitize:
            cfg.sanitize = True
        cfg.__post_init__()
        ctx.obj = RunContext(config=cfg, project=project, verbose=verbose, dry_run=dry_run)
    except Exception as err:
        console.print(f"[bold red]Configuration Error:[/bold red] {err}")
        raise typer.Exit(code=1)

    if ctx.invoked_subcommand is None:
        from build_system import interactive

        if interactive.is_interactive():
            interactive.run_menu(ctx)
        else:
            # Plain print avoids Rich re-parsing Click's own markup-free help text.
            print(ctx.get_help())
        raise typer.Exit()

if __name__ == "__main__":
    try:
        app()
    finally:
        registry.reset()
