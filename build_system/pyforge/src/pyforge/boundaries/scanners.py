import ast
import re
from collections.abc import Callable, Collection
from dataclasses import dataclass
from typing import TypeAlias

from ..config.schema import BOUNDARY_SCANNERS

_CPP_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
_CPP_DECLARATION = re.compile(r"\b(?:class|struct|union|enum(?:\s+(?:class|struct))?|namespace)\s+(?:\[\[[^\]]*\]\]\s*)?((?:\w+::)*\w+)")

ImportsOf: TypeAlias = Callable[[str], list[str]]
Resolver: TypeAlias = Callable[[str], str]
Declares: TypeAlias = Callable[[str], list[str]]


def _unchanged(raw: str) -> str:
    return raw


@dataclass(frozen=True)
class Scanner:
    suffixes: frozenset[str]
    imports: ImportsOf
    resolve: Resolver = _unchanged
    declares: Declares | None = None


SCANNERS: dict[str, Scanner] = {}


def register_scanner(name: str, suffixes: Collection[str], imports: ImportsOf, resolve: Resolver | None = None, declares: Declares | None = None) -> None:
    """How a language declares its dependencies: `imports(source_text)` returns the raw include/import strings.

    `resolve` maps one of those strings to the path it names under the scanned base (a Python `a.b` to `a/b`); the default
    treats them as paths already, which is right for C and C++ includes. `declares(source_text)` returns the names a file
    declares (types, namespaces); it is what lets a rule on a built library name a module instead of listing symbols.
    """
    SCANNERS[name] = Scanner(frozenset(suffixes), imports, resolve or _unchanged, declares)
    BOUNDARY_SCANNERS.register(name)


def _cpp_declares(text: str) -> list[str]:
    return [part for qualified in _CPP_DECLARATION.findall(text) for part in qualified.split("::") if len(part) > 1]


def _python_imports(text: str) -> list[str]:
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []
    names: list[str] = []
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            names += [alias.name for alias in node.names]
        elif isinstance(node, ast.ImportFrom) and node.level == 0 and node.module:
            names.append(node.module)
    return names


def _python_declares(text: str) -> list[str]:
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []
    return [node.name for node in ast.walk(tree) if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef))]


register_scanner("python", {".py"}, _python_imports, lambda dotted: dotted.replace(".", "/"), _python_declares)
register_scanner("cpp", {".h", ".hpp", ".cpp", ".mm"}, _CPP_INCLUDE.findall, declares=_cpp_declares)
