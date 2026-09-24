from typing import Literal, Optional

import typer
from rich.console import Console

from build_system import registry
from build_system.config import BuildConfig, LocalConfig, PROJECT_ROOT, RunContext

console = Console()
app = typer.Typer()
GROUP_HELP = "Manage local build configuration settings"
command = registry.make_group(app, group="Config")


@command(name="init", label="Init — create the default oryx.toml")
def init(
    ctx: typer.Context,
    ide: Optional[Literal["vscode", "visual_studio", "none"]] = typer.Option(
        None,
        "--ide",
        help="Generate IDE integration files (vscode | visual_studio | none) and remember the "
        "choice in oryx.local.toml (gitignored, per-developer). Defaults to the existing "
        "oryx.local.toml value, or 'none'.",
    ),
    debugger: Optional[Literal["lldb", "cppdbg"]] = typer.Option(
        None,
        "--debugger",
        help="VS Code debugger adapter used by --ide vscode (lldb = CodeLLDB, cppdbg = Microsoft "
        "C/C++). Defaults to the existing oryx.local.toml value, or 'lldb'.",
    ),
    remember: bool = typer.Option(
        True,
        "--remember/--no-remember",
        help="Save the resolved --ide/--debugger choice to oryx.local.toml for future commands.",
    ),
):
    """Initialize a default build configuration file, and optionally IDE integration."""
    run: RunContext = ctx.obj
    console.print("[bold blue]⚙️ Initializing build configuration...[/bold blue]")
    try:
        BuildConfig.init(run.config_path)
    except Exception as error:
        console.print(f"[bold red]✗ Failed to initialize configuration:[/bold red] {error}")
        raise typer.Exit(code=1)

    existing = LocalConfig.load()
    resolved_ide = ide or existing.ide_kind
    resolved_debugger = debugger or existing.debugger

    if remember:
        LocalConfig(ide_kind=resolved_ide, debugger=resolved_debugger).save()
        console.print("[green]✓ Saved IDE preference to oryx.local.toml (not committed).[/green]")

    if resolved_ide == "vscode":
        from build_system import vscode  # deferred: only imported when actually generating .vscode files

        try:
            for path in vscode.write_all(run.config, debugger=resolved_debugger):
                console.print(f"[bold green]✓ Wrote {path.relative_to(PROJECT_ROOT)}[/bold green]")
        except Exception as error:
            console.print(f"[bold red]✗ Failed to write .vscode files:[/bold red] {error}")
            raise typer.Exit(code=1)

    elif resolved_ide == "visual_studio":
        from build_system.setup.premake import ensure_premake
        from build_system.utils import run_command

        premake = ensure_premake()
        if not premake:
            raise typer.Exit(code=1)
        try:
            run_command([str(premake), "vs2022"], cwd=PROJECT_ROOT)
            console.print("[bold green]✓ Generated Visual Studio 2022 project files (premake5 vs2022).[/bold green]")
            console.print(
                "  [dim]This is independent of `forge build compile`, which still uses the "
                "[build] generator in oryx.toml (default gmake).[/dim]"
            )
        except Exception as error:
            console.print(f"[bold red]✗ Failed to generate Visual Studio project files:[/bold red] {error}")
            raise typer.Exit(code=1)
