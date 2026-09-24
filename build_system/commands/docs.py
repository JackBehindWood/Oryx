import subprocess
import sys

import typer
from rich.console import Console
from rich.markup import escape

from build_system import registry
from build_system.config import DocsTable, RunContext
from build_system.utils import missing_module_hint, remove_directory, run_command

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Build and preview the documentation site"
command = registry.make_group(app, group="Docs")

MKDOCS_PACKAGES = ["mkdocs", "mkdocs-material"]


def _docs(run: RunContext) -> DocsTable:
    return run.config.docs or DocsTable()


def _mkdocs_command(run: RunContext, *args: str) -> list[str]:
    return [sys.executable, "-m", "mkdocs", *args, "-f", str(run.project.path(_docs(run).config))]


def _require_mkdocs() -> None:
    hint = missing_module_hint("mkdocs", "docs", MKDOCS_PACKAGES)
    if hint:
        console.print(f"[bold red]✗ {escape(hint)}[/bold red]")
        raise typer.Exit(code=1)


@command(name="build", label="Build — generate the docs site (fails on broken links)")
def build_docs(ctx: typer.Context):
    """Build the documentation site into [docs] site in strict mode."""
    run: RunContext = ctx.obj
    site = _docs(run).site
    command_line = _mkdocs_command(run, "build", "--strict", "--site-dir", str(run.project.path(site)))

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    _require_mkdocs()
    try:
        with console.status("[bold blue]📚 Building docs...[/bold blue]"):
            result = run_command(command_line, cwd=run.project.root)
        if run.verbose:
            console.print(result.stdout + result.stderr)
        console.print(f"[bold green]✓ Docs built into {site}/[/bold green]\n")
    except subprocess.CalledProcessError as error:
        console.print(f"[bold red]✗ Docs build failed:[/bold red]\n{error.stderr or error.stdout}")
        raise typer.Exit(code=1)


@command(name="serve", label="Serve — live-preview the docs in a browser")
def serve_docs(
    ctx: typer.Context,
    port: int = typer.Option(8000, "--port", help="Port for the live-preview server."),
):
    """Serve the documentation with live reload until interrupted."""
    run: RunContext = ctx.obj
    command_line = _mkdocs_command(run, "serve", "--dev-addr", f"localhost:{port}")

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    _require_mkdocs()
    console.print(f"[bold blue]📚 Serving docs at http://localhost:{port} (Ctrl-C to stop)...[/bold blue]")
    try:
        run_command(command_line, cwd=run.project.root, capture_output=False)
    except KeyboardInterrupt:
        console.print("\n[dim]Docs server stopped.[/dim]")
    except subprocess.CalledProcessError:
        raise typer.Exit(code=1)


@command(name="clean", label="Clean — remove the generated docs site")
def clean_docs(ctx: typer.Context):
    """Remove the generated [docs] site directory."""
    run: RunContext = ctx.obj
    site_dir = run.project.path(_docs(run).site)

    if run.dry_run:
        console.print(f"[dim][dry-run] would remove: {site_dir}[/dim]")
        return

    remove_directory(site_dir)
    console.print("[bold green]✓ Docs site removed.[/bold green]\n")
