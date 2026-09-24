"""Generic discovery of vendored third-party libraries.

Mirrors the Lua-side convention in premake/vendor.lua's useVendorHeader():
a vendored library lives at <project>/vendor/<lib>/ (typically a git
submodule), optionally with its real header one level down at
<project>/vendor/<lib>/<lib>/ (e.g. tests/vendor/doctest/doctest/doctest.h).
Replaces what used to be hardcoded, doctest-specific constants scattered
across build_system/commands/build.py and build_system/vscode/*.py.
"""

from pathlib import Path

import typer
from rich.console import Console

from build_system.config import BuildConfig

console = Console()

# Every project directory that may contain a vendor/ folder. Extend when a
# new project is added to the workspace.
PROJECT_DIRS = ["Oryx", "Oasis", "tests"]

# Not a vendored library: where build_system/setup/vendor_scaffold.py writes
# generated <project>/vendor/premake/<lib>.lua build scripts for compiled
# (non-header-only) vendor libs, alongside the actual <lib>/ checkouts.
PREMAKE_SUBDIR_NAME = "premake"

# Only needed when the Python backend is built (see premake/python.lua).
PYTHON_VENDOR_LIBS = {"pybind11"}


def vendor_dirs(root: Path) -> list[Path]:
    """Every <project>/vendor/<lib>/ directory that currently exists."""
    dirs = []
    for project in PROJECT_DIRS:
        vendor_root = root / project / "vendor"
        if vendor_root.is_dir():
            dirs.extend(
                sorted(p for p in vendor_root.iterdir() if p.is_dir() and p.name != PREMAKE_SUBDIR_NAME)
            )
    return dirs


def missing_vendor_dirs(root: Path, cfg: BuildConfig | None = None) -> list[Path]:
    """Vendored library directories that exist but are empty — i.e. the git
    submodule hasn't been checked out yet. Python-only libs are skipped when
    cfg says Python is off."""
    skipped = PYTHON_VENDOR_LIBS if cfg is not None and not cfg.python_enabled else set()
    return [d for d in vendor_dirs(root) if d.name not in skipped and not any(d.iterdir())]


def vendor_include_paths(root: Path, cfg: BuildConfig | None = None) -> list[str]:
    """Best-effort IntelliSense include paths for every vendored lib, mirroring
    the IncludeDir entries in premake/dependencies.lua: `<lib>/include` when it
    exists (spdlog, pybind11), otherwise — like useVendorHeader's `headerSubdir`
    — the vendor dir plus a same-named subdirectory one level in (doctest).
    Python-only libs are skipped when cfg says Python is off."""
    skipped = PYTHON_VENDOR_LIBS if cfg is not None and not cfg.python_enabled else set()
    paths = []
    for d in vendor_dirs(root):
        if d.name in skipped:
            continue
        include = d / "include"
        nested = d / d.name
        headers = [include] if include.is_dir() else [d] + ([nested] if nested.is_dir() else [])
        paths.extend(f"${{workspaceFolder}}/{h.relative_to(root).as_posix()}" for h in headers)
    return paths


def ensure_vendor_dirs(root: Path, cfg: BuildConfig | None = None) -> None:
    """Verify every vendored git submodule (doctest today, and any future
    ones under <project>/vendor/<lib>/) is populated; abort with guidance if
    not. Used as the `requires_vendor=True` precondition on
    @registry.command(...) — see build_system/registry.py."""
    missing = missing_vendor_dirs(root, cfg)
    if not missing:
        return
    for d in missing:
        console.print(f"[bold red]✗ Missing vendored submodule: {d.relative_to(root)} is empty.[/bold red]")
    console.print("  [dim]Run: git submodule update --init --recursive[/dim]")
    raise typer.Exit(code=1)
