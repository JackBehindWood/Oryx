import tomllib

import typer
from rich.console import Console
from rich.markup import escape

from pyforge import registry, tomledit
from pyforge.config import LOCAL_CONFIG_NAME, ForgeConfig, LocalConfig, RunContext, SchemaError, leaf_type, parse_config, parse_scalar
from pyforge.config.schema import from_dict

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Inspect and edit forge.toml / forge.local.toml"
command = registry.make_group(app, group="Config")


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {escape(message)}[/bold red]")
    return typer.Exit(code=1)


def _split(key: str) -> list[str]:
    return [part for part in key.split(".") if part]


def _path(run: RunContext, local: bool):
    return (run.project.root / LOCAL_CONFIG_NAME) if local else run.project.config_file


def _validator(local: bool):
    return (lambda data: from_dict(LocalConfig, data, source=LOCAL_CONFIG_NAME)) if local else parse_config


def _schema(local: bool):
    return LocalConfig if local else ForgeConfig


@command(name="show", label="Show — print forge.toml (or forge.local.toml)")
def show(
    ctx: typer.Context,
    local: bool = typer.Option(False, "--local", help="Show forge.local.toml instead of forge.toml."),
):
    """Print the raw contents of the config file, comments and all."""
    run: RunContext = ctx.obj
    path = _path(run, local)
    if not path.is_file():
        raise _fail(f"{path} does not exist.")
    console.print(escape(path.read_text(encoding="utf-8")), end="")


@command(name="get", label="Get — read one config key")
def get(
    ctx: typer.Context,
    key: str = typer.Argument(..., help="Dotted key, e.g. build.jobs or targets.oasis.project."),
    local: bool = typer.Option(False, "--local", help="Read from forge.local.toml instead of forge.toml."),
):
    """Print the value at a dotted key path."""
    run: RunContext = ctx.obj
    path = _path(run, local)
    if not path.is_file():
        raise _fail(f"{path} does not exist.")
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    node = data
    parts = _split(key)
    for part in parts:
        if not isinstance(node, dict) or part not in node:
            raise _fail(f"'{key}' is not set in {path.name}.")
        node = node[part]
    console.print(escape(str(node)) if isinstance(node, dict) else tomledit.format_value(node))


@command(name="set", label="Set — write one config key")
def set_(
    ctx: typer.Context,
    key: str = typer.Argument(..., help="Dotted key, e.g. build.jobs or targets.oasis.project."),
    value: str = typer.Argument(..., help="New value, in the key's own type (e.g. true/false for a bool)."),
    local: bool = typer.Option(False, "--local", help="Write to forge.local.toml instead of forge.toml."),
):
    """Set a dotted key to a value, converted to the schema's type for that key."""
    run: RunContext = ctx.obj
    path = _path(run, local)
    parts = _split(key)
    try:
        annotation = leaf_type(_schema(local), parts)
        parsed = parse_scalar(annotation, value, key, path.name)
    except SchemaError as error:
        raise _fail(str(error))

    if run.dry_run:
        console.print(f"[dim][dry-run] would set {key} = {tomledit.format_value(parsed)} in {path}[/dim]")
        return

    try:
        tomledit.edit_file(path, lambda text: tomledit.set_value(text, parts, parsed), validate=_validator(local))
    except (tomledit.TomlEditError, SchemaError) as error:
        raise _fail(str(error))
    console.print(f"[bold green]✓ {path.name}[/bold green]: {escape(key)} = {escape(tomledit.format_value(parsed))}")


@command(name="unset", label="Unset — remove one config key")
def unset_(
    ctx: typer.Context,
    key: str = typer.Argument(..., help="Dotted key to remove."),
    local: bool = typer.Option(False, "--local", help="Remove from forge.local.toml instead of forge.toml."),
):
    """Remove a dotted key (or, for a table path, the whole table) from the config file."""
    run: RunContext = ctx.obj
    path = _path(run, local)
    parts = _split(key)

    if run.dry_run:
        console.print(f"[dim][dry-run] would unset {key} in {path}[/dim]")
        return

    try:
        tomledit.edit_file(path, lambda text: tomledit.unset(text, parts), validate=_validator(local))
    except (tomledit.TomlEditError, SchemaError) as error:
        raise _fail(str(error))
    console.print(f"[bold green]✓ Removed {key} from {path.name}[/bold green]")
