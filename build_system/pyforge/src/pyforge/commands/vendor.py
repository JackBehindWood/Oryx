from typing import Literal, Optional

import typer
from rich.console import Console

from pyforge import registry

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Renamed to `deps`"
GROUP_HIDDEN = True
command = registry.make_group(app, group="Vendor")


@command(name="add", label="Add (renamed to `deps add`)", hidden=True)
def add(
    ctx: typer.Context,
    project: str = typer.Argument(...),
    name: str = typer.Argument(...),
    url: Optional[str] = typer.Option(None, "--url"),
    kind: Literal["header-only", "static-lib"] = typer.Option("header-only", "--kind"),
    header_subdir: Optional[str] = typer.Option(None, "--header-subdir"),
    include_subdir: Optional[str] = typer.Option(None, "--include-subdir"),
    source_subdir: Optional[str] = typer.Option(None, "--source-subdir"),
    define: list[str] = typer.Option([], "--define"),
):
    """Old spelling of `forge deps add`."""
    from pyforge.commands.deps import add as deps_add

    console.print("[yellow]`forge vendor add` is now `forge deps add`; forwarding.[/yellow]")
    ctx.invoke(
        deps_add,
        ctx,
        name=name,
        local=url is None,
        submodule=url,
        path=f"{project}/vendor/{name}",
        kind="static" if kind == "static-lib" else "header",
        include=include_subdir or header_subdir,
        sources=source_subdir,
        define=define,
        requires=[],
    )
