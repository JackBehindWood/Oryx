from pathlib import Path
from typing import Optional

import typer
from rich.console import Console

from pyforge import __version__, options, plugins, registry
from pyforge.config import LOCAL_CONFIG_NAME, ForgeConfig, LocalConfig, Profile, ProjectTable, RunContext, SchemaError, load_config, load_local, validate_local
from pyforge.config.schema import parse_enum
from pyforge.plugins import PluginError
from pyforge.project import Project, ProjectNotFound

console = Console()

app = typer.Typer(
    name="forge",
    help="Forge build & tooling CLI",
    invoke_without_command=True,
    rich_markup_mode="markdown",
)

# Every pyforge/commands/<name>.py module is auto-discovered and
# attached here — dropping a new module in that directory (with its own
# `app`, `GROUP_HELP`, and `@registry.command(...)`-decorated functions)
# requires no edits to this file. See pyforge/registry.py.
for _module in registry.discover_command_modules():
    _name = _module.__name__.rsplit(".", 1)[-1]
    _root_command = getattr(_module, "ROOT_COMMAND", None)
    if _root_command is not None:
        # A Click Group always resolves its first leftover positional token as a subcommand name
        # (even with invoke_without_command=True), so a variadic argument on the group's own
        # callback can never coexist with real subcommands, and forcing allow_interspersed_args
        # still can't recover a `--`-delimited passthrough tail. A module that needs
        # `name [ARGS] [-- extra]` (see commands/test.py) mounts as a plain leaf command instead
        # of the usual add_typer() sub-group.
        app.command(name=_name, help=getattr(_module, "GROUP_HELP", ""), context_settings=getattr(_module, "ROOT_COMMAND_CONTEXT_SETTINGS", {}))(_root_command)
    elif getattr(_module, "FLAT", False):
        # Decision 6 (CLI shape): a handful of common verbs (configure/compile/all/clean/run)
        # mount directly on the root app instead of nesting under their module's own group name,
        # so `forge compile` stays a flat verb rather than `forge build compile`. The interactive
        # menu is unaffected — it groups by registry.CommandEntry.group, not by Typer mounting.
        app.registered_commands.extend(_module.app.registered_commands)
    else:
        app.add_typer(_module.app, name=_name, help=getattr(_module, "GROUP_HELP", ""), hidden=getattr(_module, "GROUP_HIDDEN", False))

# Plugin commands (e.g. `forge python stubs`, from build_system/oryx/) must be mounted onto `app`
# here, before Click parses the command line — a project's own config decides which plugins load
# (see main()'s per-invocation reload below), so this is a best-effort pass using whatever project
# is discoverable from the current directory; outside any project, plugin commands are simply absent.
try:
    _boot_project = Project.discover()
    _boot_pm = plugins.load_plugins(_boot_project.root, load_config(_boot_project.config_file, __version__).plugins.paths)
except (ProjectNotFound, SchemaError, PluginError):
    _boot_pm = plugins.get_plugin_manager()
_boot_pm.hook.forge_commands(app=app)

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
):
    """Global context setup executed before running commands."""
    try:
        project = Project.from_config(config_path) if config_path else _discover(ctx)
        cfg = _load(ctx, project)
        local, legacy_local = load_local(project.root)
        validate_local(local, cfg)
        values = options.resolve(cfg, local, with_, without)
        for define in defines:
            options.define_flag(define)
        pm = plugins.load_plugins(project.root, cfg.plugins.paths)
        ctx.obj = RunContext(
            project=project,
            config=cfg,
            local=local,
            profile=_resolve_profile(profile, cfg, local),
            options=values,
            defines=list(defines),
            verbose=verbose,
            dry_run=dry_run,
            pm=pm,
        )
    except (ProjectNotFound, SchemaError, PluginError) as err:
        console.print(f"[bold red]Configuration Error:[/bold red] {err}")
        raise typer.Exit(code=1)

    if legacy_local is not None:
        Console(stderr=True).print(
            f"[yellow]Note: reading {legacy_local.name}; rename it to {LOCAL_CONFIG_NAME} (forge never modifies the old file).[/yellow]"
        )

    if ctx.invoked_subcommand is None:
        from pyforge import interactive

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
