from pathlib import Path
from typing import Optional

import typer
from rich.console import Console

from build_system import __version__, options, registry
from build_system.config import LOCAL_CONFIG_NAME, ForgeConfig, LocalConfig, Profile, ProjectTable, RunContext, SchemaError, load_config, load_local, validate_local
from build_system.config.schema import parse_enum
from build_system.project import Project, ProjectNotFound

console = Console()

app = typer.Typer(
    name="forge",
    help="Forge build & tooling CLI",
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


def _load(ctx: typer.Context, project: Project) -> ForgeConfig:
    if not project.config_file.is_file() and ctx.invoked_subcommand == "config":
        return ForgeConfig(project=ProjectTable(name=project.root.name))
    return load_config(project.config_file, __version__)


def _resolve_profile(requested: str | None, cfg: ForgeConfig, local: LocalConfig) -> Profile:
    if requested is None:
        return local.build.default_profile or cfg.build.default_profile
    return parse_enum(Profile, requested.lower(), "--profile", "command line")


@app.callback()
def main(
    ctx: typer.Context,
    config_path: Optional[Path] = typer.Option(
        None,
        "--config",
        "-c",
        help="Path to the project's config file (default: the nearest forge.toml at or above the current directory).",
    ),
    profile: Optional[str] = typer.Option(
        None,
        "--profile",
        "-p",
        help="Build configuration profile: [debug | release | dist] (default: [build] default-profile).",
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
    with_: list[str] = typer.Option(
        [],
        "--with",
        metavar="OPTION",
        help="Turn a forge.toml [options] switch on for this run (repeatable).",
    ),
    without: list[str] = typer.Option(
        [],
        "--without",
        metavar="OPTION",
        help="Turn a forge.toml [options] switch off for this run (repeatable).",
    ),
    defines: list[str] = typer.Option(
        [],
        "-D",
        metavar="KEY[=VALUE]",
        help="Pass --KEY[=VALUE] straight to Premake (repeatable).",
    ),
    no_python: bool = typer.Option(False, "--no-python", hidden=True, help="Alias of --without python."),
    sanitize: bool = typer.Option(False, "--sanitize", hidden=True, help="Alias of --with sanitize."),
):
    """Global context setup executed before running commands."""
    try:
        project = Project.from_config(config_path) if config_path else _discover(ctx)
        cfg = _load(ctx, project)
        local, legacy_local = load_local(project.root)
        validate_local(local, cfg)
        with_ = [*with_, *(["sanitize"] if sanitize else [])]
        without = [*without, *(["python"] if no_python else [])]
        values = options.resolve(cfg, local, with_, without)
        for define in defines:
            options.define_flag(define)
        ctx.obj = RunContext(
            project=project,
            config=cfg,
            local=local,
            profile=_resolve_profile(profile, cfg, local),
            options=values,
            defines=list(defines),
            verbose=verbose,
            dry_run=dry_run,
        )
    except (ProjectNotFound, SchemaError) as err:
        console.print(f"[bold red]Configuration Error:[/bold red] {err}")
        raise typer.Exit(code=1)

    if legacy_local is not None:
        Console(stderr=True).print(
            f"[yellow]Note: reading {legacy_local.name}; rename it to {LOCAL_CONFIG_NAME} (forge never modifies the old file).[/yellow]"
        )

    if ctx.invoked_subcommand is None:
        from build_system import interactive

        if interactive.is_interactive():
            ctx.obj.interactive = True
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
