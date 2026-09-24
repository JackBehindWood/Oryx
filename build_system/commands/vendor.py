from typing import Literal, Optional

import typer
from rich.console import Console

from build_system import registry
from build_system.config import RunContext
from build_system.setup import vendor_scaffold
from build_system.vendor import PROJECT_DIRS, vendor_dirs

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Add and scaffold vendored third-party libraries"
command = registry.make_group(app, group="Vendor")


@command(name="add", label="Add — vendor a new third-party library")
def add(
    ctx: typer.Context,
    project: str = typer.Argument(..., help=f"Project to vendor into: one of {', '.join(PROJECT_DIRS)}."),
    name: str = typer.Argument(..., help="Library name — becomes the vendor directory and Premake project name."),
    url: Optional[str] = typer.Option(
        None,
        "--url",
        help="Git repo URL to add as a submodule at PROJECT/vendor/NAME. "
        "Omit if the submodule is already checked out.",
    ),
    kind: Literal["header-only", "static-lib"] = typer.Option(
        "header-only",
        "--kind",
        help="header-only: wires a useVendorHeader() call into PROJECT/premake5.lua. "
        "static-lib: scaffolds PROJECT/vendor/premake/NAME.lua as its own compiled project, "
        "included from the root premake5.lua's group \"Dependencies\".",
    ),
    header_subdir: Optional[str] = typer.Option(
        None,
        "--header-subdir",
        help="For --kind header-only: subdirectory within vendor/NAME/ containing the real "
        "header, if the repo nests it (e.g. doctest -> vendor/doctest/doctest/doctest.h).",
    ),
    include_subdir: Optional[str] = typer.Option(
        None,
        "--include-subdir",
        help="For --kind static-lib: subdirectory within vendor/NAME/ containing its public "
        "headers, if the repo nests them (e.g. spdlog -> vendor/spdlog/include).",
    ),
    source_subdir: Optional[str] = typer.Option(
        None,
        "--source-subdir",
        help="For --kind static-lib: subdirectory within vendor/NAME/ to compile, if the repo "
        "mixes in example/test/bench sources you don't want globbed (e.g. spdlog -> "
        "vendor/spdlog/src).",
    ),
    define: Optional[list[str]] = typer.Option(
        None,
        "--define",
        help="For --kind static-lib: extra preprocessor define(s), added to both the vendored "
        "project and PROJECT/premake5.lua (e.g. spdlog needs SPDLOG_COMPILED_LIB). Repeatable.",
    ),
):
    """Vendor a new third-party library as a git submodule and wire it into Premake."""
    run: RunContext = ctx.obj
    root = run.project.root
    if project not in PROJECT_DIRS:
        console.print(f"[bold red]✗ Unknown project '{project}'. Must be one of: {', '.join(PROJECT_DIRS)}[/bold red]")
        raise typer.Exit(code=1)

    path = vendor_scaffold.vendor_path(root, project, name)

    if path.exists():
        console.print(f"[dim]{path.relative_to(root)} already exists, skipping submodule add.[/dim]")
    elif url:
        try:
            vendor_scaffold.add_git_submodule(root, url, project, name)
            console.print(f"[bold green]✓ Added submodule {project}/vendor/{name}[/bold green]")
        except Exception as error:
            console.print(f"[bold red]✗ git submodule add failed:[/bold red] {error}")
            raise typer.Exit(code=1)
    else:
        console.print(f"[bold red]✗ {project}/vendor/{name} doesn't exist yet. Pass --url to add it as a submodule.[/bold red]")
        raise typer.Exit(code=1)

    if kind == "header-only":
        edited, call = vendor_scaffold.insert_header_only_usage(root, project, name, header_subdir)
        if edited:
            console.print(f"[bold green]✓ {project}/premake5.lua now calls {call}[/bold green]")
        else:
            console.print(f"[yellow]⚠️ Couldn't safely edit {project}/premake5.lua. Add this line yourself:[/yellow]")
            console.print(f"  [dim]{call}[/dim]")
    else:
        script_path = vendor_scaffold.write_static_lib_script(
            root, project, name, include_subdir=include_subdir, source_subdir=source_subdir, defines=define
        )
        console.print(f"[bold green]✓ Wrote {script_path.relative_to(root)}[/bold green]")

        edited, snippets = vendor_scaffold.insert_static_lib_wiring(
            root, project, name, include_subdir=include_subdir, defines=define
        )
        if edited:
            console.print(f"[bold green]✓ Wired into {project}/premake5.lua (include, includedirs, links)[/bold green]")
        else:
            console.print(f"[yellow]⚠️ Couldn't safely edit {project}/premake5.lua. Add these yourself:[/yellow]")
            for snippet in snippets:
                console.print(f"  [dim]{snippet}[/dim]")

    console.print(f"\n[bold blue]{len(vendor_dirs(root))} vendored librar{'y' if len(vendor_dirs(root)) == 1 else 'ies'} tracked.[/bold blue]")
