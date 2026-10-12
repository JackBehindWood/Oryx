import subprocess
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import TypeAlias

from ..config.schema import ARTIFACT_TOOLS

SymbolReader: TypeAlias = Callable[[Path], list[str]]


@dataclass(frozen=True)
class ArtifactTool:
    defined: SymbolReader
    undefined: SymbolReader | None = None


ARTIFACT_SCANNERS: dict[str, ArtifactTool] = {}


def register_artifact_tool(name: str, defined: SymbolReader, undefined: SymbolReader | None = None) -> None:
    """How a build artifact is inspected: `defined(path)` returns the symbols it defines, `undefined(path)` the ones it needs from elsewhere."""
    ARTIFACT_SCANNERS[name] = ArtifactTool(defined, undefined)
    ARTIFACT_TOOLS.register(name)


def _nm_symbols(path: Path, flags: list[str]) -> list[str]:
    output = subprocess.run(["nm", "-C", *flags, str(path)], capture_output=True, text=True, check=True).stdout
    symbols = []
    for line in output.splitlines():
        line = line.strip()
        if not line or line.endswith(":"):
            continue
        parts = line.split(None, 2)
        symbols.append(parts[2] if len(parts) == 3 else parts[-1])
    return symbols


register_artifact_tool(
    "nm",
    lambda path: _nm_symbols(path, ["--defined-only"]),
    lambda path: _nm_symbols(path, ["-u"]),
)
