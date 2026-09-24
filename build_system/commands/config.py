from typing import Literal, Optional

import typer
from rich.console import Console

from build_system import registry
from build_system import tomledit
from build_system.config import LOCAL_CONFIG_NAME, RunContext, save_local

console = Console()
app = typer.Typer()
GROUP_HELP = "Manage local build configuration settings"
command = registry.make_group(app, group="Config")


@command(name="init", label="Init — create a default forge.toml")
def init(
    ctx: typer.Context,
    ide: Optional[Literal["vscode", "visual_studio", "none"]] = typer.Option(
        None,
        "--ide",
        help="Generate IDE integration files (vscode | visual_studio | none) and remember the "
        "choice in forge.local.toml (gitignored, per-developer). Defaults to the existing "
        "[editor] kind, or 'none'.",
    ),
    debugger: Optional[Literal["lldb", "cppdbg"]] = typer.Option(
        None,
        "--debugger",
        help="VS Code debugger adapter used by --ide vscode (lldb = CodeLLDB, cppdbg = Microsoft "
        "C/C++). Defaults to the existing [editor] debugger, or 'lldb'.",
    ),
    remember: bool = typer.Option(
        True,
        "--remember/--no-remember",
        help="Save the resolved --ide/--debugger choice to forge.local.toml for future commands.",
    ),
):
    """Initialize a default build configuration file, and optionally IDE integration."""
    run: RunContext = ctx.obj
    root = run.project.root
    config_file = run.project.config_file
    console.print("[bold blue]⚙️ Initializing build configuration...[/bold blue]")
    if config_file.exists():
        console.print(f"⚠️ Configuration file already exists at: {config_file}")
    else:
        config_file.write_text(tomledit.dumps({"project": {"name": root.name}}), encoding="utf-8")
        console.print(f"✓ Created default build configuration at: {config_file}")

    resolved_ide = ide or run.local.editor.kind
    resolved_debugger = debugger or run.local.editor.debugger

    if remember:
        save_local(root, "editor", {"kind": str(resolved_ide), "debugger": str(resolved_debugger)})
        console.print(f"[green]✓ Saved IDE preference to {LOCAL_CONFIG_NAME} (not committed).[/green]")

    if resolved_ide == "vscode":
        from build_system import vscode  # deferred: only imported when actually generating .vscode files

        try:
            for path in vscode.write_all(run, debugger=resolved_debugger):
                console.print(f"[bold green]✓ Wrote {path.relative_to(root)}[/bold green]")
        except Exception as error:
            console.print(f"[bold red]✗ Failed to write .vscode files:[/bold red] {error}")
            raise typer.Exit(code=1)

    elif resolved_ide == "visual_studio":
        from build_system.setup.premake import ensure_premake
        from build_system.utils import run_command

        premake = ensure_premake(run.project.premake_bin_dir, run.config.premake.version)
        if not premake:
            raise typer.Exit(code=1)
        try:
            run_command([str(premake), "vs2022"], cwd=root)
            console.print("[bold green]✓ Generated Visual Studio 2022 project files (premake5 vs2022).[/bold green]")
            console.print(
                "  [dim]This is independent of `forge build compile`, which still uses the "
                "[premake] generator in forge.toml (default gmake).[/dim]"
            )
        except Exception as error:
            console.print(f"[bold red]✗ Failed to generate Visual Studio project files:[/bold red] {error}")
            raise typer.Exit(code=1)
