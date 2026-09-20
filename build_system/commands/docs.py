import subprocess

import typer
from rich.console import Console

from build_system import registry
from build_system.config import MKDOCS_CONFIG, PROJECT_ROOT, SITE_DIR, RunContext
from build_system.utils import remove_directory, run_command

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Build and preview the documentation site"
command = registry.make_group(app, group="Docs")


def _mkdocs_command(*args: str) -> list[str]:
    return ["uv", "run", "--group", "docs", "mkdocs", *args, "-f", str(MKDOCS_CONFIG)]


def _missing_uv() -> None:
    console.print("[bold red]✗ 'uv' was not found on PATH.[/bold red] The docs toolchain runs through it.")
    raise typer.Exit(code=1)


@command(name="build", label="Build — generate the docs site (fails on broken links)")
def build_docs(ctx: typer.Context):
    """Build the documentation site into site/ in strict mode."""
    run: RunContext = ctx.obj
    command_line = _mkdocs_command("build", "--strict")

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    try:
        with console.status("[bold blue]📚 Building docs...[/bold blue]"):
            result = run_command(command_line, cwd=PROJECT_ROOT)
        if run.verbose:
            console.print(result.stdout + result.stderr)
        console.print(f"[bold green]✓ Docs built into {SITE_DIR.relative_to(PROJECT_ROOT)}/[/bold green]\n")
    except FileNotFoundError:
        _missing_uv()
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
    command_line = _mkdocs_command("serve", "--dev-addr", f"localhost:{port}")

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    console.print(f"[bold blue]📚 Serving docs at http://localhost:{port} (Ctrl-C to stop)...[/bold blue]")
    try:
        run_command(command_line, cwd=PROJECT_ROOT, capture_output=False)
    except FileNotFoundError:
        _missing_uv()
    except KeyboardInterrupt:
        console.print("\n[dim]Docs server stopped.[/dim]")
    except subprocess.CalledProcessError:
        raise typer.Exit(code=1)


@command(name="clean", label="Clean — remove the generated docs site")
def clean_docs(ctx: typer.Context):
    """Remove the generated site/ directory."""
    run: RunContext = ctx.obj

    if run.dry_run:
        console.print(f"[dim][dry-run] would remove: {SITE_DIR}[/dim]")
        return

    remove_directory(SITE_DIR)
    console.print("[bold green]✓ Docs site removed.[/bold green]\n")
