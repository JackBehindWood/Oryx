import re
from collections.abc import Callable
from dataclasses import dataclass

from ..config.schema import BOUNDARY_SCANNERS

_CPP_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)


@dataclass(frozen=True)
class Scanner:
    suffixes: frozenset[str]
    imports: Callable[[str], list[str]]


SCANNERS: dict[str, Scanner] = {}


def register_scanner(name: str, suffixes: set[str], imports: Callable[[str], list[str]]) -> None:
    """How a language declares its dependencies: `imports(source_text)` returns the raw include/import strings."""
    SCANNERS[name] = Scanner(frozenset(suffixes), imports)
    BOUNDARY_SCANNERS.register(name)


register_scanner("cpp", {".h", ".hpp", ".cpp", ".mm"}, _CPP_INCLUDE.findall)
