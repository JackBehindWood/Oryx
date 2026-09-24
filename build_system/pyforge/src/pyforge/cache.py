import os
import sys
from pathlib import Path


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
