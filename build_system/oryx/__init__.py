"""The Oryx pyforge plugin: everything specific to this engine, kept out of the generic build_system.

Behind pyforge's hookspecs (build_system/plugins.py): the embedded Python backend's Premake args,
PythonConfig.h, the `import oryx` .pth file, `forge python stubs`, and the [tool.oryx] schema.
Loaded via forge.toml's `[plugins] paths = ["build_system/oryx"]`.
"""

from rich.console import Console

from build_system.api import hookimpl

from . import commands
from .python_env import PythonEnvError, premake_python_options, python_build_info, write_python_config
from .python_extension import install_extension_pth, remove_extension_pth

console = Console()

__all__ = ["PythonEnvError"]


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
    from build_system import workspace

    python_enabled = ctx.options.get("python", False)
    if python_enabled and not ctx.options.get("sanitize", False):
        ws = workspace.load(ctx.project)
        install_extension_pth(ws.target_path("OryxPython", ctx.profile).parent)
    elif remove_extension_pth():
        # A sanitized oryx.so needs the ASan runtime preloaded, which a plain `python` never has.
        reason = "a --sanitize extension can't be imported by plain python" if python_enabled else "this build has no Python extension"
        console.print(f"[dim]Removed the venv's `import oryx` path: {reason}; a normal build restores it.[/dim]\n")


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
