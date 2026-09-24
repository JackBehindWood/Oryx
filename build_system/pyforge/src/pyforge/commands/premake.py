from typing import Optional

import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry, tomledit
from pyforge.config import RunContext, parse_config
from pyforge.premake.install import check_local_premake, ensure_premake, get_premake_executable, installed_version, latest_release_version, resolve_bin_dir, update_premake

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Manage the local Premake5 install"
command = registry.make_group(app, group="Premake")


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {escape(message)}[/bold red]")
    return typer.Exit(code=1)


@command(name="status", label="Status — show the local Premake5 install")
def status(ctx: typer.Context):
    """Report whether the pinned Premake5 version is installed, and where."""
    run: RunContext = ctx.obj
    bin_dir = resolve_bin_dir(run.project, run.config.premake.path, run.config.premake.version)
    executable = get_premake_executable(bin_dir)
    pinned = run.config.premake.version
    version = installed_version(executable)
    if version:
        console.print(f"[green]✓ premake5:[/green] {version} at {executable}")
        if pinned not in version:
            console.print(f"[yellow]  Pinned version is v{pinned}. Run 'forge premake install' to fetch it.[/yellow]")
    else:
        console.print(f"[yellow]✗ premake5: not installed locally (expected {executable}).[/yellow]")
        console.print("  [dim]Run 'forge configure' or 'forge premake install' to install it.[/dim]")


@command(name="install", label="Install — download the pinned Premake5 version")
def install(
    ctx: typer.Context,
    version: Optional[str] = typer.Option(None, "--version", help="Defaults to forge.toml's [premake] version."),
):
    """Download and checksum-verify a Premake5 version into the shared user cache."""
    run: RunContext = ctx.obj
    target = version or run.config.premake.version
    bin_dir = resolve_bin_dir(run.project, run.config.premake.path, target)

    if run.dry_run:
        console.print(f"[dim][dry-run] would install premake5 v{target} into {bin_dir}[/dim]")
        return

    if check_local_premake(bin_dir):
        console.print(f"[green]✓ premake5 v{target} is already installed at {get_premake_executable(bin_dir)}[/green]")
        return

    if not ensure_premake(bin_dir, target):
        raise typer.Exit(code=1)


@command(name="update", label="Update — fetch a newer release and rewrite [premake] version")
def update(
    ctx: typer.Context,
    version: Optional[str] = typer.Option(None, "--version", help="Defaults to the latest GitHub release."),
):
    """Download `--version` (or the latest release), verify it, and pin forge.toml to it."""
    run: RunContext = ctx.obj
    target = version
    if target is None:
        target = latest_release_version()
        if target is None:
            raise _fail("Could not reach GitHub to find the latest Premake5 release; pass --version.")

    bin_dir = resolve_bin_dir(run.project, run.config.premake.path, target)

    if run.dry_run:
        console.print(f"[dim][dry-run] would update premake5 to v{target} and set {escape('[premake]')} version in forge.toml[/dim]")
        return

    if not update_premake(bin_dir, target):
        raise typer.Exit(code=1)

    if target != run.config.premake.version:
        try:
            tomledit.edit_file(run.project.config_file, lambda text: tomledit.set_value(text, ["premake", "version"], target), validate=parse_config)
        except (tomledit.TomlEditError, ValueError) as error:
            raise _fail(str(error))
        console.print(f"[bold green]✓ forge.toml[/bold green] {escape('[premake]')}: version = {escape(tomledit.format_value(target))}")
