"""Decorator-based command registration.

Each pyforge/commands/<name>.py module calls `make_group(app, group=...)`
once to get a `command` decorator, then applies it to every function it wants
exposed on the CLI. One `@command(...)` call registers the function with BOTH
its module's Typer sub-app and the interactive menu (pyforge/interactive.py)
in source-declaration order — no separate MENU_ENTRIES list, no manual
GROUPS wiring, no per-module add_typer() call in main.py.
"""

from __future__ import annotations

import dataclasses
import functools
import importlib
import itertools
import pkgutil
from typing import Callable

import typer

_counter = itertools.count()


@dataclasses.dataclass
class CommandEntry:
    group: str
    label: str
    func: Callable
    order: int
    hidden: bool = False
    requires_dependencies: bool = False


_REGISTRY: list[CommandEntry] = []


def make_group(app: typer.Typer, group: str):
    """Return a `@command(...)` decorator bound to this module's Typer app
    and interactive-menu group.

    Usage, inside e.g. pyforge/commands/build.py:

        app = typer.Typer(no_args_is_help=True)
        command = registry.make_group(app, group="Build")

        @command(name="configure", label="Configure — generate Premake build files")
        def configure(ctx: typer.Context): ...

    Extra keyword args:
        hidden: keep the command callable from the CLI but exclude it from
            the interactive menu (for future internal/plumbing commands).
        requires_dependencies: make sure every forge.toml [dependencies]
            entry this run needs is present before the command's body,
            unless the command was invoked with --dry-run.
    """
    def command(*, name: str, label: str, hidden: bool = False, requires_dependencies: bool = False, **typer_kwargs):
        def decorator(func: Callable) -> Callable:
            target = _with_dependency_check(func) if requires_dependencies else func
            wrapped = app.command(name, **typer_kwargs)(target)
            _REGISTRY.append(
                CommandEntry(
                    group=group,
                    label=label,
                    func=wrapped,
                    order=next(_counter),
                    hidden=hidden,
                    requires_dependencies=requires_dependencies,
                )
            )
            return wrapped

        return decorator

    return command


def register_entry(*, group: str, label: str, func: Callable, hidden: bool = False) -> None:
    """Add `func` to the interactive menu under `group` without also registering it as a Click subcommand.

    For a module whose CLI behaviour lives entirely on its Typer app's own
    `@app.callback(...)` (e.g. `forge test [SUITE...]`, where a variadic
    Argument on a Group callback makes any further `@command(...)`
    subcommand unreachable — Click's parser consumes every remaining
    positional token into that Argument before it ever looks for a
    subcommand name) rather than on a `@command(...)` subcommand.
    """
    _REGISTRY.append(CommandEntry(group=group, label=label, func=func, order=next(_counter), hidden=hidden))


def _with_dependency_check(func: Callable) -> Callable:
    @functools.wraps(func)
    def guarded(ctx, *args, **kwargs):
        from pyforge.commands.deps import ensure_or_exit

        if not ctx.obj.dry_run:
            ensure_or_exit(ctx.obj)
        return func(ctx, *args, **kwargs)

    return guarded


def discover_command_modules() -> list:
    """Import every pyforge/commands/<name>.py module, triggering their
    top-level `@command(...)` decorators (and therefore full registration)
    as a side effect. Safe to call more than once — importlib caches
    modules, so re-importing does not re-run decorators or duplicate
    registry entries.

    Discovery order is alphabetical by filename (a guarantee of
    pkgutil._iter_file_finder_modules, which sorts filenames before
    yielding) — not necessarily the order you'd want in the menu. Add a
    module-level `GROUP_ORDER = <int>` attribute to pin a group's position
    explicitly; otherwise groups display in (alphabetical) discovery order.
    """
    from pyforge import commands as commands_pkg

    modules = []
    for info in pkgutil.iter_modules(commands_pkg.__path__, prefix=f"{commands_pkg.__name__}."):
        modules.append(importlib.import_module(info.name))
    return modules


def _group_order(modules) -> dict[str, int]:
    return {_group_name(module): getattr(module, "GROUP_ORDER", i) for i, module in enumerate(modules)}


def _group_name(module) -> str:
    return module.__name__.rsplit(".", 1)[-1].capitalize()


def groups_in_order() -> list[str]:
    """Menu/CLI group names in display order, skipping groups that end up
    with no visible (non-hidden) commands."""
    order = _group_order(discover_command_modules())
    visible_groups = {entry.group for entry in _REGISTRY if not entry.hidden}
    first_seen = {}
    for entry in _REGISTRY:
        first_seen.setdefault(entry.group, entry.order)
    return sorted(visible_groups, key=lambda group: (order.get(group, 999), first_seen[group]))


def entries_for_group(group: str) -> list[CommandEntry]:
    return sorted((e for e in _REGISTRY if e.group == group and not e.hidden), key=lambda e: e.order)


def reset() -> None:
    """Clear all registered commands.

    A single CLI invocation exits right after `app()` runs, so this isn't
    needed to avoid a real leak in normal use — but it keeps registry state
    predictable the moment `pyforge` is imported more than once per
    process (e.g. a pytest run exercising discover_command_modules() across
    multiple test files/fixtures).
    """
    global _counter
    _REGISTRY.clear()
    _counter = itertools.count()
