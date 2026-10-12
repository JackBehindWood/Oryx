import re
from collections.abc import Iterable
from dataclasses import dataclass
from fnmatch import fnmatchcase
from pathlib import Path
from typing import TYPE_CHECKING, Literal, TypeAlias

from ..config.schema import SchemaError
from .artifacts import ARTIFACT_SCANNERS
from .optional import OptionalPart, Options
from .rules import _Known
from .select import compile_selector
from .symbols import Symbols
from .violation import Violation

if TYPE_CHECKING:
    from .check import Scan
    from .spec import BoundarySpec, Selection

LibraryRuleKind: TypeAlias = Literal["export", "require", "import", "symbols", "pattern"]


def _items(options: Options | None) -> list[str | OptionalPart]:
    return [] if options is None else [options] if isinstance(options, (str, OptionalPart)) else list(options)


def _names(options: Options | None) -> list[str]:
    return [item.option if isinstance(item, OptionalPart) else item for item in _items(options)]


def reason_of(*options: Options | None) -> str:
    """The `because` of the first OptionalPart among the options, used when a rule gives none."""
    return next((item.because for group in options for item in _items(group) if isinstance(item, OptionalPart) and item.because), "")


def requires_of(when_on: Options | None, when_off: Options | None) -> list[str]:
    return [*_names(when_on), *(f"!{option}" for option in _names(when_off))]


@dataclass(frozen=True)
class LibraryRule:
    kind: LibraryRuleKind
    text: str
    because: str
    message: str
    requires: tuple[str, ...]
    selection: "Selection | None" = None
    patterns: tuple[str | Symbols, ...] = ()
    declared: str = ""


class Library:
    """Checks on a built artifact (a static library, shared library or executable), read once per check however many rules apply.

    The `when_on` / `when_off` options given to `spec.library(...)` apply to every rule; a rule can give its own to replace them.
    Every rule takes `because=` (appended to the generated sentence) and `message=` (a template; `{default}` extends the sentence;
    placeholders: {default} {because} {artifact} {symbol} {name} {count} {rule}).
    """

    def __init__(self, spec: "BoundarySpec", path: str, tool: str = "nm", requires: Iterable[str] = (), because: str = "") -> None:
        self.spec = spec
        self.because = because
        self.path = path
        self.tool = tool
        self.requires = tuple(requires)
        self.rules: list[LibraryRule] = []

    def rebound(self, spec: "BoundarySpec") -> "Library":
        """A copy of this library, and its rules, that belongs to `spec`."""
        copy = Library(spec, self.path, self.tool, self.requires, self.because)
        copy.rules = list(self.rules)
        return copy

    def _add(self, kind: LibraryRuleKind, text: str, because: str, message: str, when_on: Options | None, when_off: Options | None, **extra) -> None:
        from .spec import _declared

        own = when_on is not None or when_off is not None
        requires = tuple(requires_of(when_on, when_off)) if own else self.requires
        because = because or (reason_of(when_on, when_off) if own else self.because)
        self.rules.append(LibraryRule(kind, text, because, message, requires, declared=_declared(), **extra))

    def must_not_export(self, selection: "Selection | Symbols", *, because: str = "", message: str = "", when_on: Options | None = None, when_off: Options | None = None) -> None:
        """Nothing declared only by `selection` (its types and namespaces, derived from the source) may be defined in the artifact.

        Give `spec.symbols(...)` instead for code that is not in the scanned tree: those namespaces and prefixes may not be defined.
        """
        if isinstance(selection, Symbols):
            self._add("symbols", f"{self.path} !defines {selection.label}", because, message, when_on, when_off, patterns=(selection,))
            return
        self._add("export", f"{self.path} !defines {selection.label}", because, message, when_on, when_off, selection=selection)

    def must_export(self, selection: "Selection", *, because: str = "", message: str = "", when_on: Options | None = None, when_off: Options | None = None) -> None:
        """The artifact must define at least one thing declared only by `selection`, so a module can't silently drop out of a build."""
        self._add("require", f"{self.path} defines {selection.label}", because, message, when_on, when_off, selection=selection)

    def must_not_import(self, *symbol_globs: "str | Symbols", because: str = "", message: str = "", when_on: Options | None = None, when_off: Options | None = None) -> None:
        """The artifact must not need these symbols from elsewhere: `spec.symbols(...)` objects or globs (`"*glfw*"`)."""
        self._add("import", f"{self.path} !imports {' '.join(map(str, symbol_globs))}", because, message, when_on, when_off, patterns=symbol_globs)

    def must_not_export_symbols(self, *symbol_globs: "str | Symbols", because: str = "", message: str = "", when_on: Options | None = None, when_off: Options | None = None) -> None:
        """The artifact must not define symbols matching these globs (`oryx::Graphics::*`); for names no source file declares."""
        self._add("symbols", f"{self.path} !defines {' '.join(map(str, symbol_globs))}", because, message, when_on, when_off, patterns=symbol_globs)

    def deny_pattern(self, regex: str, *, because: str = "", message: str = "", when_on: Options | None = None, when_off: Options | None = None) -> None:
        """Escape hatch: no symbol, defined or needed, may match this regex."""
        self._add("pattern", f"{self.path} !~ {regex}", because, message, when_on, when_off, patterns=(regex,))


def _qualified_names(symbol: str, namespace: str) -> set[str]:
    if not namespace:
        return set(re.findall(r"\w+", symbol))
    names: set[str] = set()
    for match in re.finditer(rf"\b{re.escape(namespace)}((?:::\w+)+)", symbol):
        names.update(match[1].split("::")[1:])
    return names


def _report(rule: LibraryRule, artifact: str, symbol: str, name: str, count: int, sentence: str) -> str:
    default = f"{sentence} ({rule.because})" if rule.because else sentence
    template = rule.message or "{default}"
    return template.format_map(_Known(default=default, because=rule.because, artifact=artifact, symbol=symbol, name=name, count=count, rule=rule.text))


def _fold(hits: dict[str, list[str]]) -> list[tuple[str, str, int]]:
    return [(name, symbols[0], len(symbols)) for name, symbols in sorted(hits.items())]


def check_library(library: Library, project_root: Path, options: dict[str, bool], scan: "Scan") -> list[Violation]:
    from ..deps.resolve import requirements_met

    active = [rule for rule in library.rules if requirements_met(list(rule.requires), options)]
    if not active:
        return []
    artifacts = sorted(project_root.glob(library.path))
    if not artifacts:
        return [Violation(library.path, "no artifact found; build first")]
    tool = ARTIFACT_SCANNERS[library.tool]
    namespace = library.spec.namespace
    violations: list[Violation] = []
    for artifact in artifacts:
        shown = artifact.relative_to(project_root).as_posix()
        defined: list[str] | None = None
        undefined: list[str] | None = None
        for rule in active:
            if rule.kind in ("export", "require", "symbols", "pattern") and defined is None:
                defined = tool.defined(artifact)
            if rule.kind in ("import", "pattern") and undefined is None:
                if tool.undefined is None and rule.kind == "import":
                    raise SchemaError(f"boundaries: artifact tool '{library.tool}' cannot list undefined symbols, so must_not_import is unavailable")
                undefined = tool.undefined(artifact) if tool.undefined else []
            found = _apply(rule, shown, library, scan, namespace, defined or [], undefined or [])
            violations += [Violation(shown, message, shown, 0, rule.text, library.spec.name) for message in found]
    return violations


def _apply(rule: LibraryRule, artifact: str, library: Library, scan: "Scan", namespace: str, defined: list[str], undefined: list[str]) -> list[str]:
    name = Path(artifact).name
    if rule.kind in ("export", "require"):
        assert rule.selection is not None
        if not namespace:
            raise SchemaError("boundaries: must_not_export / must_export need BoundarySpec(namespace=...) to know which symbols belong to the source")
        forbidden = scan.exclusive_to(compile_selector(list(rule.selection.words), {}))
        hits: dict[str, list[str]] = {}
        for symbol in defined:
            found = sorted(_qualified_names(symbol, namespace) & forbidden)
            if found:
                hits.setdefault(found[0], []).append(symbol)
        if rule.kind == "require":
            return [] if hits else [_report(rule, name, "", rule.selection.label, 0, f"{name} defines nothing from {rule.selection.label}; the module is missing from the build")]
        return [_report(rule, name, symbol, found, count, f"{name} defines {symbol}" + (f" and {count - 1} more" if count > 1 else "")) for found, symbol, count in _fold(hits)]
    if rule.kind in ("symbols", "import"):
        symbols = defined if rule.kind == "symbols" else undefined
        verb = "defines" if rule.kind == "symbols" else "imports"
        hits = {}
        for item in rule.patterns:
            if isinstance(item, Symbols):
                for symbol in symbols:
                    if claimed := item.find(symbol):
                        hits.setdefault(f"{item.label}: {claimed}", []).append(symbol)
                continue
            hits[item] = [symbol for symbol in symbols if fnmatchcase(symbol, item) or fnmatchcase(symbol.split("(")[0], item)]
        return [_report(rule, name, symbol, glob, count, f"{name} {verb} {symbol}" + (f" and {count - 1} more" if count > 1 else "")) for glob, symbol, count in _fold({g: s for g, s in hits.items() if s})]
    regex = rule.patterns[0]
    assert isinstance(regex, str)
    pattern = re.compile(regex)
    seen = sorted({symbol.strip() for symbol in (*defined, *undefined) if pattern.search(symbol)})
    return [_report(rule, name, symbol, symbol, 1, f"{name} exports {symbol}") for symbol in seen]
