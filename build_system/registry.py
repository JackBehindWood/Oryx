"""Decorator-based command registration.

Each build_system/commands/<name>.py module calls `make_group(app, group=...)`
once to get a `command` decorator, then applies it to every function it wants
exposed on the CLI. One `@command(...)` call registers the function with BOTH
its module's Typer sub-app and the interactive menu (build_system/interactive.py)
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
    requires_vendor: bool = False


_REGISTRY: list[CommandEntry] = []


def make_group(app: typer.Typer, group: str):
    """Return a `@command(...)` decorator bound to this module's Typer app
    and interactive-menu group.

    Usage, inside e.g. build_system/commands/build.py:

        app = typer.Typer(no_args_is_help=True)
        command = registry.make_group(app, group="Build")

        @command(name="configure", label="Configure — generate Premake build files")
        def configure(ctx: typer.Context): ...

    Extra keyword args:
        hidden: keep the command callable from the CLI but exclude it from
            the interactive menu (for future internal/plumbing commands).
        requires_vendor: run build_system.vendor.ensure_vendor_dirs() before
            the command's body, unless the command was invoked with
            --dry-run — generalizes the "is the doctest submodule checked
            out?" guard so any command that needs vendored headers gets it
            declaratively instead of a hand-written check in its body.
    """
    def command(*, name: str, label: str, hidden: bool = False, requires_vendor: bool = False, **typer_kwargs):
        def decorator(func: Callable) -> Callable:
            target = _with_vendor_check(func) if requires_vendor else func
            wrapped = app.command(name, **typer_kwargs)(target)
            _REGISTRY.append(
                CommandEntry(
                    group=group,
                    label=label,
                    func=wrapped,
                    order=next(_counter),
                    hidden=hidden,
                    requires_vendor=requires_vendor,
                )
            )
            return wrapped

        return decorator

    return command


def _with_vendor_check(func: Callable) -> Callable:
    @functools.wraps(func)
    def guarded(ctx, *args, **kwargs):
        from build_system.vendor import ensure_vendor_dirs

        run = ctx.obj
        if not run.dry_run:
            ensure_vendor_dirs(run.project.root, run.config)
        return func(ctx, *args, **kwargs)

    return guarded


def discover_command_modules() -> list:
    """Import every build_system/commands/<name>.py module, triggering their
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
    from build_system import commands as commands_pkg

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
    predictable the moment `build_system` is imported more than once per
    process (e.g. a pytest run exercising discover_command_modules() across
    multiple test files/fixtures).
    """
    global _counter
    _REGISTRY.clear()
    _counter = itertools.count()
