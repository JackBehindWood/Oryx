import typer
from rich.console import Console

from pyforge import registry
from pyforge.config import RunContext
from pyforge.setup.premake import get_premake_executable, installed_version, update_premake

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
    run: RunContext = ctx.obj
    bin_dir = run.project.premake_bin_dir
    pinned = run.config.premake.version
    if update:
        if not update_premake(bin_dir, pinned):
            raise typer.Exit(code=1)
        return

    executable = get_premake_executable(bin_dir)
    version = installed_version(executable)
    if version:
        console.print(f"[green]✓ premake5:[/green] {version} at {executable}")
        if pinned not in version:
            console.print(f"[yellow]  Pinned version is v{pinned}. Run with --update to refresh.[/yellow]")
    else:
        console.print(f"[yellow]✗ premake5: not installed locally (expected {executable}).[/yellow]")
        console.print("  [dim]Run 'forge build configure' or 'forge setup premake --update' to install it.[/dim]")
