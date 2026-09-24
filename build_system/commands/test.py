import subprocess

import typer
from rich.console import Console

from build_system import registry
from build_system.config import RunContext
from build_system.utils import run_command

console = Console()
app = typer.Typer(invoke_without_command=True)
GROUP_HELP = "Execute test binaries"
command = registry.make_group(app, group="Test")


@app.callback()
def test_callback(ctx: typer.Context):
    """Execute test binaries."""
    if ctx.invoked_subcommand is None:
        ctx.invoke(run_tests, ctx)
        raise typer.Exit()


@command(name="run", label="Run — execute the test suite")
def run_tests(ctx: typer.Context):
    """Execute the project test suite."""
    run: RunContext = ctx.obj
    cfg = run.config

    test_path = cfg.test_suite_path()

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {test_path}[/dim]")
        return

    console.print(f"[bold blue]🧪 Running tests ({cfg.profile})...[/bold blue]")

    if not test_path.exists():
        console.print(f"[bold red]✗ Test binary missing at:[/bold red] {test_path}")
        console.print("  [dim]Run 'forge build compile' first.[/dim]")
        raise typer.Exit(code=1)

    try:
        with console.status("[bold blue]Running tests...[/bold blue]"):
            result = run_command([str(test_path), "--test-suite-exclude=benchmark"])
        if run.verbose and result.stdout:
            console.print(result.stdout)
        console.print("[bold green]✓ Tests passed[/bold green]\n")
    except subprocess.CalledProcessError as error:
        # doctest reports failures on stdout; stderr is usually empty.
        output = error.stdout or error.stderr or "(no output)"
        console.print(f"[bold red]✗ Tests failed:[/bold red]\n{output}")
        raise typer.Exit(code=1)


@command(name="benchmark", label="Benchmark — run performance micro-benchmarks")
def run_benchmarks(ctx: typer.Context):
    """Run the performance micro-benchmark suite (excluded from `test run`)."""
    run: RunContext = ctx.obj
    cfg = run.config

    test_path = cfg.test_suite_path()

    if run.dry_run:
        console.print(f"[dim][dry-run] would run: {test_path} --test-suite=benchmark[/dim]")
        return

    console.print(f"[bold blue]📊 Running benchmarks ({cfg.profile})...[/bold blue]")

    if not test_path.exists():
        console.print(f"[bold red]✗ Test binary missing at:[/bold red] {test_path}")
        console.print("  [dim]Run 'forge build compile' first.[/dim]")
        raise typer.Exit(code=1)

    try:
        result = run_command([str(test_path), "--test-suite=benchmark"])
        console.print(result.stdout)
        console.print("[bold green]✓ Benchmarks complete[/bold green]\n")
    except subprocess.CalledProcessError as error:
        output = error.stdout or error.stderr or "(no output)"
        console.print(f"[bold red]✗ Benchmarks failed:[/bold red]\n{output}")
        raise typer.Exit(code=1)
