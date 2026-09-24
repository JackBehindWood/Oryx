import typer
from rich.console import Console

from build_system import registry
from build_system.config import RunContext
from build_system.deps.resolve import DependencyError, ensure

console = Console()
app = typer.Typer(no_args_is_help=True)
GROUP_HELP = "Third-party dependencies declared in forge.toml [dependencies]"
command = registry.make_group(app, group="Deps")


def ensure_or_exit(run: RunContext) -> None:
    try:
        ensure(run)
    except DependencyError as error:
        console.print(f"[bold red]✗ {error}[/bold red]")
        raise typer.Exit(code=1)
