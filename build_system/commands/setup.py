import typer
from rich.console import Console

from build_system import registry
from build_system.setup.premake import PREMAKE_VERSION, get_premake_executable, installed_version, update_premake

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Manage local toolchain installs (Premake, etc.)"
command = registry.make_group(app, group="Setup")


@command(name="premake", label="Premake — show the local Premake5 install status")
def premake(
    ctx: typer.Context,
    update: bool = typer.Option(
        False,
        "--update",
        help="Force re-download the pinned Premake5 version, even if already installed.",
    ),
):
    """Show, or update, the locally-vendored Premake5 install."""
    if update:
        if not update_premake():
            raise typer.Exit(code=1)
        return

    executable = get_premake_executable()
    version = installed_version(executable)
    if version:
        console.print(f"[green]✓ premake5:[/green] {version} at {executable}")
        if PREMAKE_VERSION not in version:
            console.print(f"[yellow]  Pinned version is v{PREMAKE_VERSION}. Run with --update to refresh.[/yellow]")
    else:
        console.print(f"[yellow]✗ premake5: not installed locally (expected {executable}).[/yellow]")
        console.print("  [dim]Run 'build build configure' or 'build setup premake --update' to install it.[/dim]")
