from typing import Optional

import typer
from rich.console import Console

from build_system import registry, workspace
from build_system.config import LOCAL_CONFIG_NAME, Debugger, RunContext, save_local
from build_system.editors import registry as editor_registry

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Generate editor integration files from the Premake export"
command = registry.make_group(app, group="Editor")

REMEMBER_HELP = "Save the editor choice to forge.local.toml (gitignored, per-developer)."


def save_preference(run: RunContext, kind: str, debugger: str) -> None:
    save_local(run.project.root, "editor", {"kind": kind, "debugger": debugger})
    console.print(f"[green]✓ Saved editor preference to {LOCAL_CONFIG_NAME} (not committed).[/green]")


def _ensure_export(ctx: typer.Context) -> None:
    run: RunContext = ctx.obj
    if workspace.load(run.project) is None:
        from build_system.commands.build import configure

        ctx.invoke(configure, ctx)


def write_vscode(ctx: typer.Context, debugger: str, remember: bool) -> None:
    run: RunContext = ctx.obj
    if run.dry_run:
        console.print("[dim][dry-run] would write .vscode/{tasks,settings,c_cpp_properties,launch}.json[/dim]")
        return
    if remember:
        save_preference(run, "vscode", debugger)
    _ensure_export(ctx)
    from build_system.editors import vscode

    try:
        for path in vscode.write_all(run, debugger=debugger):
            console.print(f"[bold green]✓ Wrote {path.relative_to(run.project.root)}[/bold green]")
    except (OSError, ValueError, workspace.WorkspaceError) as error:
        console.print(f"[bold red]✗ Failed to write .vscode files:[/bold red] {error}")
        raise typer.Exit(code=1)


def write_vs2022(ctx: typer.Context, remember: bool) -> None:
    import subprocess

    from build_system.commands.build import premake_args
    from build_system.commands.deps import ensure_or_exit
    from build_system.deps.resolve import write_premake_config
    from build_system.setup.premake import ensure_premake, get_premake_executable
    from build_system.utils import run_command

    run: RunContext = ctx.obj
    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {get_premake_executable(run.project.premake_bin_dir)} vs2022 {' '.join(premake_args(run))}[/dim]")
        return
    if remember:
        save_preference(run, "visual_studio", str(run.local.editor.debugger))
    ensure_or_exit(run)
    premake = ensure_premake(run.project.premake_bin_dir, run.config.premake.version)
    if not premake:
        raise typer.Exit(code=1)
    write_premake_config(run)
    try:
        run_command([str(premake), "vs2022", *premake_args(run)], cwd=run.project.root)
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Failed to generate Visual Studio project files:[/bold red]\n{error.stderr}")
        raise typer.Exit(code=1)
    console.print("[bold green]✓ Generated Visual Studio 2022 project files (premake5 vs2022).[/bold green]")
    console.print(f"  [dim]`forge build compile` still uses the [premake] generator ({run.config.premake.generator}).[/dim]")


@command(name="vscode", label="VS Code — write .vscode tasks, launch, IntelliSense, settings")
def vscode(
    ctx: typer.Context,
    debugger: Optional[Debugger] = typer.Option(
        None, "--debugger", help="lldb (CodeLLDB) or cppdbg (Microsoft C/C++). Defaults to the saved [editor] debugger."
    ),
    remember: bool = typer.Option(True, "--remember/--no-remember", help=REMEMBER_HELP),
):
    """Generate or merge .vscode/{tasks,settings,c_cpp_properties,launch}.json, configuring first if needed."""
    run: RunContext = ctx.obj
    editor_registry.editor_command("vscode")(ctx, str(debugger or run.local.editor.debugger), remember)


@command(name="vs2022", label="Visual Studio 2022 — generate a solution with premake5 vs2022")
def vs2022(
    ctx: typer.Context,
    remember: bool = typer.Option(True, "--remember/--no-remember", help=REMEMBER_HELP),
):
    """Generate a Visual Studio 2022 solution with the same options and dependencies as the build."""
    editor_registry.editor_command("visual_studio")(ctx, remember)


def _register_builtins() -> None:
    editor_registry.register("vscode", write_vscode)
    editor_registry.register("visual_studio", write_vs2022)


_register_builtins()
