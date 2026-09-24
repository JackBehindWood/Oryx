from typing import Literal, Optional

import typer
from rich.console import Console

from build_system import registry, tomledit
from build_system.config import RunContext

console = Console()
app = typer.Typer()
GROUP_HELP = "Manage local build configuration settings"
command = registry.make_group(app, group="Config")


@command(name="init", label="Init — create a default forge.toml")
def init(
    ctx: typer.Context,
    ide: Optional[Literal["vscode", "visual_studio", "none"]] = typer.Option(None, "--ide", hidden=True),
    debugger: Optional[Literal["lldb", "cppdbg"]] = typer.Option(None, "--debugger", hidden=True),
    remember: bool = typer.Option(True, "--remember/--no-remember", hidden=True),
):
    """Create a minimal forge.toml in the current directory. Editor files: `forge editor vscode|vs2022`."""
    run: RunContext = ctx.obj
    config_file = run.project.config_file
    if config_file.exists():
        console.print(f"⚠️ Configuration file already exists at: {config_file}")
    else:
        config_file.write_text(tomledit.dumps({"project": {"name": run.project.root.name}}), encoding="utf-8")
        console.print(f"✓ Created default build configuration at: {config_file}")

    from build_system.commands import editor

    if ide is None or ide == "none":
        if remember and (ide or debugger):
            editor.save_preference(run, ide or run.local.editor.kind, debugger or str(run.local.editor.debugger))
        return

    console.print(f"[dim]`config init --ide` is now `forge editor {'vscode' if ide == 'vscode' else 'vs2022'}`; forwarding.[/dim]")
    if ide == "vscode":
        editor.write_vscode(ctx, debugger or str(run.local.editor.debugger), remember)
    else:
        editor.write_vs2022(ctx, remember)
