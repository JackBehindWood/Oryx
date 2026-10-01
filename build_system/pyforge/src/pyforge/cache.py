import contextlib
import json
import os
import sys
import threading
import time
from pathlib import Path
from typing import Callable

from .utils import rmtree_force, write_text_lf


def user_cache_dir() -> Path:
    """~/.cache/pyforge (XDG_CACHE_HOME), ~/Library/Caches/pyforge on macOS, or
    %LOCALAPPDATA%/pyforge on Windows; PYFORGE_CACHE overrides all three."""
    override = os.environ.get("PYFORGE_CACHE")
    if override:
        return Path(override)
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Caches" / "pyforge"
    if sys.platform == "win32":
        base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
        return Path(base) / "pyforge"
    base = os.environ.get("XDG_CACHE_HOME") or str(Path.home() / ".cache")
    return Path(base) / "pyforge"


def premake_dir(version: str) -> Path:
    """Shared across every project and worktree that pins this version; survives `forge clean`.
    [premake] path in forge.toml overrides this (e.g. a Linux arm64 build with no official asset)."""
    return user_cache_dir() / "premake" / version


PINS_NAME = ".pins.json"
_pins_lock = threading.Lock()


def deps_root() -> Path:
    return user_cache_dir() / "deps"


def deps_dir(name: str, pin: str) -> Path:
    """Shared across every project and worktree that pins this name+pin; `forge deps clean-cache` is the only deleter."""
    return deps_root() / name / pin


def atomic_extract(dest: Path, extractor: Callable[[Path], None]) -> bool:
    """Fill `dest` via `extractor(tmp)` then rename into place; False when another writer got there first."""
    if dest.exists():
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_name(f"{dest.name}.tmp-{os.getpid()}-{threading.get_ident()}")
    rmtree_force(tmp, ignore_errors=True)
    try:
        extractor(tmp)
        os.replace(tmp, dest)
    except OSError:
        if not dest.exists():
            raise
        return False
    finally:
        rmtree_force(tmp, ignore_errors=True)
    return True


def read_pins() -> dict[str, dict[str, str]]:
    """{project_root: {name: pin}} for every project that fetched into the cache."""
    try:
        return json.loads((deps_root() / PINS_NAME).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


@contextlib.contextmanager
def _pins_file_lock():
    """Serialises pin updates across processes (two worktrees syncing at once), not just threads."""
    path = deps_root() / f"{PINS_NAME}.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a+b") as handle:
        if sys.platform == "win32":
            import msvcrt

            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_LOCK, 1)
            try:
                yield
            finally:
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(handle, fcntl.LOCK_EX)
            try:
                yield
            finally:
                fcntl.flock(handle, fcntl.LOCK_UN)


def _replace_retrying(source: Path, dest: Path, attempts: int = 10) -> None:
    for attempt in range(attempts):
        try:
            os.replace(source, dest)
            return
        except PermissionError:
            if attempt == attempts - 1:
                raise
            time.sleep(0.05)


def record_pin(project_root: Path, name: str, pin: str) -> None:
    with _pins_lock, _pins_file_lock():
        pins = read_pins()
        pins.setdefault(str(project_root), {})[name] = pin
        path = deps_root() / PINS_NAME
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_name(f"{path.name}.tmp-{os.getpid()}-{threading.get_ident()}")
        write_text_lf(tmp, json.dumps(pins, indent=1, sort_keys=True) + "\n")
        _replace_retrying(tmp, path)


def unused_entries() -> list[Path]:
    """Cache entries no still-existing project pins."""
    live = {(name, pin) for root, named in read_pins().items() if Path(root).is_dir() for name, pin in named.items()}
    root = deps_root()
    if not root.is_dir():
        return []
    return sorted(
        entry
        for name_dir in root.iterdir()
        if name_dir.is_dir()
        for entry in name_dir.iterdir()
        if entry.is_dir() and (name_dir.name, entry.name) not in live
    )
