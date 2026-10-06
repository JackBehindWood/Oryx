import subprocess
from collections.abc import Callable
from pathlib import Path

from ..config.schema import ARTIFACT_TOOLS

ARTIFACT_SCANNERS: dict[str, Callable[[Path], list[str]]] = {}


def register_artifact_tool(name: str, lines: Callable[[Path], list[str]]) -> None:
    """How a build artifact is inspected: `lines(path)` returns the text lines a `deny` regex is searched in."""
    ARTIFACT_SCANNERS[name] = lines
    ARTIFACT_TOOLS.register(name)


def _nm(path: Path) -> list[str]:
    return subprocess.run(["nm", "-C", str(path)], capture_output=True, text=True, check=True).stdout.splitlines()


register_artifact_tool("nm", _nm)
