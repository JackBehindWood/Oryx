import typer
from rich.console import Console

from pyforge import registry
from pyforge.api import hookimpl

console = Console()
commands_app = typer.Typer(no_args_is_help=True)
command = registry.make_group(commands_app, group="Dummy")

DUMMY_PREMAKE_ARGS = ["--dummy-flag=1"]


@command(name="hello", label="Hello — print the [tool.dummy] greeting")
def hello(ctx: typer.Context):
    """Print forge.toml's [tool.dummy] greeting."""
    greeting = ctx.obj.config.tool.get("dummy", {}).get("greeting")
    if not greeting:
        console.print("[bold red]✗ Set [tool.dummy] greeting in forge.toml.[/bold red]")
        raise typer.Exit(code=1)
    console.print(greeting)


@hookimpl
def forge_commands(app):
    app.add_typer(commands_app, name="dummy", help="Dummy plugin commands")


@hookimpl
def forge_premake_args(ctx):
    return DUMMY_PREMAKE_ARGS


@hookimpl
def forge_config_schema():
    return {"greeting": str}
