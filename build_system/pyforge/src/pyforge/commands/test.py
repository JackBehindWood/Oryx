import subprocess
import sys
from pathlib import Path
from typing import Optional

import typer
from rich.console import Console

from pyforge import registry, runners
from pyforge.config import RunContext, Suite
from pyforge.config.schema import suggestion
from pyforge.deps.resolve import requirements_met
from pyforge.utils import child_env, missing_module_hint, run_command, stream_command

console = Console()
GROUP_HELP = "Execute test binaries"

# See pyforge/main.py's discovery loop: this mounts as a plain leaf command
# (`ROOT_COMMAND`), not the usual add_typer() sub-group, because a Click Group can't
# combine a suite-selecting argument with a `--`-delimited passthrough tail.
ROOT_COMMAND_CONTEXT_SETTINGS = {"allow_extra_args": True, "ignore_unknown_options": True}


def _fail(message: str) -> typer.Exit:
    console.print(f"[bold red]✗ {message}[/bold red]")
    return typer.Exit(code=1)


def _test_binary(run: RunContext) -> Path:
    from pyforge.commands.build import require_workspace

    return require_workspace(run).target_path(run.config.tests.project, run.profile)


def _default_suites(run: RunContext) -> list[str]:
    return [name for name, suite in run.config.tests.suites.items() if suite.default]


def _resolve_names(run: RunContext, requested: list[str]) -> list[str]:
    """No names, or the deprecated bare `run` alias, mean every default suite."""
    if not requested or requested == ["run"]:
        if requested == ["run"]:
            console.print("[dim]Note: 'test run' is deprecated; use 'forge test'.[/dim]")
        return _default_suites(run)
    names = run.config.tests.suites
    for name in requested:
        if name not in names:
            available = ", ".join(names) or "(none in forge.toml)"
            raise _fail(f"Unknown test suite '{name}'{suggestion(name, names)} (available: {available})")
    return requested


def _selected_suites(run: RunContext, requested: list[str]) -> dict[str, Suite]:
    """The requested suites, minus any whose `requires` this run's options don't meet."""
    selected = {}
    for name in _resolve_names(run, requested):
        suite = run.config.tests.suites[name]
        if not requirements_met(suite.requires, run.options):
            console.print(f"[yellow]Skipping '{name}': requires {', '.join(suite.requires)}[/yellow]")
            continue
        selected[name] = suite
    return selected


def _suite_kinds(run: RunContext, suites: dict[str, Suite]) -> dict[str, runners.RunnerKind]:
    return {name: runners.runner_kind(run.project.path(suite.dir)) for name, suite in suites.items()}


def _dirs_by_kind(suites: dict[str, Suite], kinds: dict[str, runners.RunnerKind]) -> dict[runners.RunnerKind, list[str]]:
    grouped: dict[runners.RunnerKind, list[str]] = {}
    for name, suite in suites.items():
        grouped.setdefault(kinds[name], []).append(suite.dir)
    return grouped


def _build_command(run: RunContext, kind: runners.RunnerKind, dirs: list[str], extra: list[str], *, list_only: bool) -> list[str]:
    binary = _test_binary(run) if kind == "doctest" else None
    if kind == "pytest":
        hint = missing_module_hint("pytest", "test", ["pytest"])
        if hint:
            raise _fail(hint)
    return runners.runner_for(kind).command(binary, dirs, extra, list_only=list_only)


def run_suites(
    ctx: typer.Context,
    suites: Optional[str] = typer.Argument(
        None,
        metavar="[SUITE[,SUITE...]]",
        help="Comma-separated forge.toml [tests] suite names to run (default: every suite with default = true). "
        "Arguments after `--` are passed to the runner, when every selected suite shares one.",
    ),
    list_: bool = typer.Option(False, "--list", help="List the selected suites' test cases/items instead of running them."),
):
    """Run (or list) the project's test suites."""
    run: RunContext = ctx.obj
    if run.config.tests is None:
        raise _fail("No [tests] table in forge.toml.")

    requested = [name for name in (suites or "").split(",") if name]
    selected = _selected_suites(run, requested)
    if not selected:
        console.print("[yellow]No suites selected.[/yellow]")
        return

    kinds = _suite_kinds(run, selected)
    extra = list(ctx.args)
    distinct_kinds = set(kinds.values())
    if extra and len(distinct_kinds) > 1:
        mixed = ", ".join(f"{name} ({kind})" for name, kind in kinds.items())
        raise _fail(f"-- arguments need every selected suite to share one runner; got: {mixed}")

    dirs_by_kind = _dirs_by_kind(selected, kinds)

    if run.dry_run:
        for kind, dirs in dirs_by_kind.items():
            command_line = _build_command(run, kind, dirs, extra, list_only=list_)
            console.print(f"[dim][dry-run] would run: {' '.join(command_line)}[/dim]")
        return

    if "doctest" in dirs_by_kind:
        binary = _test_binary(run)
        if not binary.exists():
            console.print(f"[bold red]✗ Test binary missing at:[/bold red] {binary}")
            console.print("  [dim]Run 'forge compile' first.[/dim]")
            raise typer.Exit(code=1)

    plugin_env: dict[str, str] = {}
    for suite in selected.values():
        for result in run.pm.hook.forge_test_env(ctx=run, suite=suite):
            if result:
                plugin_env.update(result)
    env = child_env(Path(sys.executable).parent, plugin_env or None)

    if list_:
        for kind, dirs in dirs_by_kind.items():
            command_line = _build_command(run, kind, dirs, [], list_only=True)
            try:
                result = run_command(command_line, cwd=run.project.root, env=env)
                console.print(result.stdout)
            except subprocess.CalledProcessError as error:
                console.print(error.stdout or error.stderr or "(no output)")
        return

    console.print(f"[bold blue]🧪 Running tests ({', '.join(selected)})...[/bold blue]")
    failed = False
    for kind, dirs in dirs_by_kind.items():
        command_line = _build_command(run, kind, dirs, extra, list_only=False)
        if stream_command(command_line, cwd=run.project.root, env=env, console=console) != 0:
            failed = True

    if failed:
        console.print("[bold red]✗ Tests failed[/bold red]")
        raise typer.Exit(code=1)
    console.print("[bold green]✓ Tests passed[/bold green]\n")


ROOT_COMMAND = run_suites
registry.register_entry(group="Test", label="Run — execute the test suites", func=run_suites)
