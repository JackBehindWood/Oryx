"""The Oryx pyforge plugin: everything specific to this engine, kept out of the generic pyforge.

Behind pyforge's hookspecs (pyforge/plugins.py): the embedded Python backend's Premake args,
PythonConfig.h, the `import oryx` .pth file, `forge python stubs`, and the [tool.oryx] schema.
Loaded via forge.toml's `[plugins] paths = ["build_system/oryx"]`.
"""

from rich.console import Console

from pyforge.api import hookimpl

from . import commands
from .python_env import PythonEnvError, premake_python_options, python_build_info, write_python_config
from .python_extension import install_extension_pth, remove_extension_pth

console = Console()

__all__ = ["PythonEnvError"]


def cook_shaders(ctx):
    """Compile every registered shader after a graphics build, so a broken shader fails the build and the store is warm."""
    import subprocess
    import sys

    from pyforge import workspace

    if not ctx.options.get("graphics", False) or ctx.dry_run:
        return
    ws = workspace.load(ctx.project)
    target = ctx.config.targets.get(ctx.config.project.default_target)
    if ws is None or target is None:
        return
    binary = ws.target_path(target.project, ctx.profile)
    console.print("[bold blue]🎨 Cooking shaders...[/bold blue]")
    result = subprocess.run([str(binary), "--cook-shaders"], cwd=ctx.project.root, capture_output=True, text=True)
    if result.returncode != 0:
        console.print(f"[bold red]✗ Shader cook failed:[/bold red]\n{result.stdout}{result.stderr}")
        sys.exit(1)
    console.print("[bold green]✓ Shaders cooked[/bold green]\n")


@hookimpl
def forge_commands(app):
    app.add_typer(commands.app, name="python", help=commands.GROUP_HELP)


@hookimpl
def forge_premake_args(ctx):
    return premake_python_options(ctx.options.get("python", False))


@hookimpl
def forge_pre_configure(ctx):
    if ctx.options.get("python", False):
        write_python_config(python_build_info(), ctx.project.build_dir)


@hookimpl
def forge_post_compile(ctx):
    from pyforge import workspace

    python_enabled = ctx.options.get("python", False)
    if python_enabled and not ctx.options.get("sanitize", False):
        ws = workspace.load(ctx.project)
        install_extension_pth(ws.target_path("OryxPython", ctx.profile).parent)
    elif remove_extension_pth():
        # A sanitized oryx.so needs the ASan runtime preloaded, which a plain `python` never has.
        reason = "a --sanitize extension can't be imported by plain python" if python_enabled else "this build has no Python extension"
        console.print(f"[dim]Removed the venv's `import oryx` path: {reason}; a normal build restores it.[/dim]\n")

    cook_shaders(ctx)


@hookimpl
def forge_post_clean(ctx):
    if remove_extension_pth():
        console.print("[green]✓ Removed the venv's `import oryx` path[/green]")


@hookimpl
def forge_test_env(ctx, suite):
    if ctx.options.get("python", False):
        return {"ORYX_REQUIRE_EXTENSION": "1"}
    return None


@hookimpl
def forge_config_schema():
    return {"stubs-dir": str}
