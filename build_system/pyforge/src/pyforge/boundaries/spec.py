import sys
from collections.abc import Iterable
from dataclasses import dataclass, field, replace
from datetime import date
from pathlib import Path
from typing import TYPE_CHECKING, Self, TypeAlias, TypeVar

from ..config.schema import BoundariesTable, SchemaError, suggestion
from .optional import OptionalPart
from .rules import Message, Rule, parse_rules
from .symbols import Symbols
from .scanners import SCANNERS
from .select import Selector, compile_selector

if TYPE_CHECKING:
    from .library import Library, Options


@dataclass(frozen=True)
class Selection:
    """A named set of paths. Declare them on a BoundarySpec (`module`, `files`, `pattern`, `group`) and pass the objects to rules."""

    label: str
    words: tuple[str, ...]
    required: bool = True
    kind: str = "module"
    spec: "BoundarySpec | None" = field(default=None, compare=False, repr=False)

    def __or__(self, other: "Selection") -> "Selection":
        return Selection(f"{self.label} | {other.label}", self.words + other.words, False, "union", self.spec or other.spec)

    def __str__(self) -> str:
        return self.label

    def _owner(self) -> "BoundarySpec":
        if self.spec is None:
            raise SchemaError(f"boundaries: '{self.label}' is not attached to a BoundarySpec; use the spec's rule methods")
        return self.spec

    def must_not_include(self, *targets: "Selections", because: str = "", unless: "Selections" = (), message: Message = "") -> None:
        """Files here must not include any of `targets`, except from files in `unless`."""
        self._owner()._forbid(self, targets, because, unless, message, _declared())

    def must_not_be_included(self, *, because: str = "", unless: "Selections" = (), message: Message = "") -> None:
        """Nothing may include this part, except files in `unless`."""
        owner = self._owner()
        owner._forbid(owner.everything, self, because, unless, message, _declared())

    def only_included_by(self, *sources: "Selections", because: str = "", message: Message = "") -> None:
        """Only files in `sources` may include this."""
        self._owner()._only(self, sources, because, message, _declared())


Selections: TypeAlias = "Selection | Iterable[Selections]"
PartT = TypeVar("PartT", bound=Selection)


def _selection(value: Selections, what: str) -> Selection:
    if isinstance(value, Selection):
        return value
    if isinstance(value, str):
        raise TypeError(f"{what} must be declared with spec.module(...), spec.files(...), spec.pattern(...) or spec.group(...), not the string {value!r}")
    members = [_selection(member, what) for member in value]
    return Selection(" | ".join(member.label for member in members), tuple(word for member in members for word in member.words), False, "union")


def _declared() -> str:
    frame = sys._getframe(2)
    return f"{Path(frame.f_code.co_filename).name}:{frame.f_lineno}"


@dataclass(frozen=True)
class Waiver:
    """A known violation parked until `expires`; it turns into an error after that day."""

    source: Selector
    target: Selector
    label: str
    because: str
    expires: date
    declared: str


@dataclass(frozen=True)
class Module(Selection):
    """A discovered directory: usable in rules, and `module.Child` reaches the directory below it."""

    tree: "ModuleTree | None" = field(default=None, compare=False, repr=False)
    depth: int = 0

    def __getattr__(self, name: str) -> "Module":
        if name.startswith("_"):
            raise AttributeError(name)
        if self.tree is None:
            raise AttributeError(f"'{self.label}' was declared with module(); only spec.discover(...) modules have children")
        return self.tree.child(self.label, name, self.depth)


class ModuleTree:
    """The directories under the scanned base, as attributes: `tree.Interface.GUI`. Each one is registered as a normal module."""

    def __init__(self, spec: "BoundarySpec", directory: Path, depth: int) -> None:
        self._spec = spec
        self._directory = directory
        self._depth = depth

    def child(self, parent: str, name: str, parent_depth: int = 0) -> Module:
        path = f"{parent}/{name}" if parent else name
        here = self._directory / parent
        available = sorted(entry.name for entry in here.iterdir() if entry.is_dir()) if here.is_dir() else []
        if parent_depth >= self._depth:
            raise AttributeError(f"'{path}' is deeper than discover(depth={self._depth}); declare it with spec.module('{path}')")
        if not (self._directory / path).exists():
            raise AttributeError(f"no directory '{name}' in '{parent or self._directory.name}'{suggestion(name, available)} (available: {', '.join(available)})")
        existing = self._spec._find(path)
        if isinstance(existing, Module):
            if existing.tree is not None:
                return existing
            bound = replace(existing, tree=self, depth=parent_depth + 1)
            self._spec.parts[self._spec.parts.index(existing)] = bound
            return bound
        if existing is not None:
            raise AttributeError(f"'{path}' is already declared as a {existing.kind}, not a module")
        return self._spec._part(Module(path, (path,), True, "module", self._spec, self, parent_depth + 1))

    def __getattr__(self, name: str) -> Module:
        if name.startswith("_"):
            raise AttributeError(name)
        return self.child("", name)

    def __getitem__(self, path: str) -> Module:
        parent = ""
        found: Module | None = None
        for index, name in enumerate(part for part in path.split("/") if part):
            found = self.child(parent, name, index)
            parent = found.label
        if found is None:
            raise KeyError(path)
        return found


class BoundarySpec:
    """Layering rules for one language: what may include what, plus checks on built libraries.

    Declare the parts of the tree once (`module`, `modules`, `files`, `pattern`, `group`, or `discover` for every directory),
    then relate the objects with `forbid`, `only`, `layers`, `isolate`, `peers`, `above` or the fluent
    `part.must_not_include(...)`. Rules take those objects, never path strings, so a misspelt part is a NameError and a part that
    matches nothing is reported by `forge boundaries check`.

    A violation prints a generated sentence naming the real modules, plus the rule's `because`. A rule's `message` replaces it, and
    `{default}` inside a template extends it; `BoundarySpec(message=...)` sets the template for every rule without its own.
    Placeholders: {default} {because} {source} {target} {file} {line} {path} {raw} {rule}.
    """

    def __init__(self, root: str = ".", base: str = "", scan: str = "cpp", *, name: str = "", namespace: str = "", message: Message = "") -> None:
        if scan not in SCANNERS:
            raise SchemaError(f"boundaries scan '{scan}' is not registered (available: {', '.join(SCANNERS)})")
        self.root = root
        self.base = base
        self.scan = scan
        self.name = name or scan
        self.namespace = namespace
        self.message = message
        self.sets: dict[str, list[str]] = {}
        self.rules: list[Rule] = []
        self.waivers: list[Waiver] = []
        self.libraries: list[Library] = []
        self.optionals: list[OptionalPart] = []
        self.parts: list[Selection] = []
        self.everything = Selection("everything", ("**",), False, "everything", self)
        self._leaves: tuple[int, list[tuple[Selection, Selector]]] = (-1, [])
        self._names: dict[str, str] = {}

    @classmethod
    def from_table(cls, table: BoundariesTable) -> Self:
        from .library import Library

        spec = cls(table.root, table.base, table.scan)
        spec.sets = {name: list(words) for name, words in table.sets.items()}
        spec.rules = [replace(rule, declared="forge.toml") for rule in parse_rules(table)]
        for artifact in table.artifacts:
            library = Library(spec, artifact.path, artifact.tool, artifact.requires)
            library.deny_pattern(artifact.deny, because=artifact.why)
            spec.libraries.append(library)
        return spec

    @classmethod
    def joined(cls, *specs: "BoundarySpec", name: str = "") -> Self:
        """A new spec holding everything the given specs declare; they must scan the same tree with the same language."""
        if not specs:
            raise SchemaError("boundaries: joined() needs at least one spec")
        first = specs[0]
        return cls(first.root, first.base, first.scan, name=name or first.name, namespace=first.namespace, message=first.message).merge(*specs)

    def merge(self, *others: "BoundarySpec") -> Self:
        """Add everything `others` declare (parts, rules, waivers, libraries) to this spec, which is returned. The others are left untouched.

        The specs must share root, base and language; a part declared in both must be identical, otherwise it is an error. A
        selection taken from another spec still adds rules to that spec when used through its own methods (`part.must_not_include`);
        pass it to this spec's methods (`spec.forbid(part, ...)`) instead, or declare the rules before merging.
        """
        for other in others:
            if other is self:
                continue
            for field_name in ("root", "base", "scan"):
                if getattr(other, field_name) != getattr(self, field_name):
                    raise SchemaError(f"boundaries: cannot merge '{other.name}' into '{self.name}': {field_name} differs ({getattr(other, field_name)!r} vs {getattr(self, field_name)!r})")
            if self.namespace and other.namespace and self.namespace != other.namespace:
                raise SchemaError(f"boundaries: cannot merge '{other.name}' into '{self.name}': namespace differs ({other.namespace!r} vs {self.namespace!r})")
            self.namespace = self.namespace or other.namespace
            self.message = self.message or other.message
            for part in other.parts:
                known = self._find(part.label)
                if known is None:
                    self._part(self._rebound(part))
                elif (known.words, known.kind) != (part.words, part.kind):
                    raise SchemaError(f"boundaries: '{part.label}' is declared differently in '{self.name}' and '{other.name}'")
            for label, words in other.sets.items():
                self.sets.setdefault(label, list(words))
            self.rules += [rule for rule in other.rules if rule not in self.rules]
            self.waivers += [waiver for waiver in other.waivers if waiver not in self.waivers]
            for library in other.libraries:
                self.libraries.append(library.rebound(self))
            for optional in other.optionals:
                if all(known.option != optional.option for known in self.optionals):
                    self.optionals.append(optional)
        return self

    def _rebound(self, part: Selection) -> Selection:
        if isinstance(part, Module) and part.tree is not None:
            return replace(part, spec=self, tree=ModuleTree(self, part.tree._directory, part.tree._depth))
        return replace(part, spec=self)

    def _find(self, label: str) -> Selection | None:
        return next((part for part in self.parts if part.label == label), None)

    def _part(self, selection: PartT) -> PartT:
        if self._find(selection.label) is not None:
            raise SchemaError(f"boundaries: '{selection.label}' is declared twice")
        selection = selection if selection.spec is self else replace(selection, spec=self)
        self.parts.append(selection)
        self.sets[selection.label] = list(selection.words)
        return selection

    def module(self, path: str, *, name: str = "", allow_empty: bool = False) -> Module:
        """A directory (or file) of the scanned tree and everything under it, e.g. `module("Interface/Canvas")`."""
        return self._part(Module(name or path, (path,), not allow_empty, "module"))

    def modules(self, *paths: str | Iterable[str], allow_empty: bool = False) -> list[Module]:
        """Several modules at once, from paths or lists of paths, in order: `core, game = spec.modules("Core", "Game")`."""
        flat = [leaf for path in paths for leaf in ([path] if isinstance(path, str) else path)]
        return [self.module(path, allow_empty=allow_empty) for path in flat]

    def files(self, name: str, *globs: str, allow_empty: bool = False) -> Selection:
        """Paths matching any of the globs, relative to the scanned base: `*`, `**`, `?` and `{a,b}` alternatives; a glob without a `/` matches a file name at any depth.

        `allow_empty` is for guard rules over paths that may not exist yet; otherwise matching nothing is reported as a mistake.
        """
        return self._part(Selection(name, tuple(globs), not allow_empty, "files"))

    def pattern(self, name: str, regex: str, *, allow_empty: bool = False) -> Selection:
        """Paths matching a regex searched anywhere in the path, for what a glob can't say."""
        return self._part(Selection(name, (f"re:{regex}",), not allow_empty, "pattern"))

    def group(self, name: str, *members: Selections) -> Selection:
        """Several declared parts treated as one, so a rule can name them together."""
        return self._part(Selection(name, _selection(members, f"group '{name}' members").words, False, "group"))

    def discover(self, project_root: Path, *, depth: int = 2) -> ModuleTree:
        """Every directory under the scanned base as attributes (`tree.Interface.GUI`); a typo lists the real ones."""
        return ModuleTree(self, project_root / self.root / self.base, depth)

    def name_of(self, path: str) -> str:
        """The most specific declared module containing `path`, or '' when none does."""
        if self._leaves[0] != len(self.parts):
            leaves = [(part, compile_selector(list(part.words), {})) for part in self.parts if part.kind == "module"]
            self._leaves = (len(self.parts), leaves)
            self._names.clear()
        if path in self._names:
            return self._names[path]
        best = max(((len(part.words[0]), part.label) for part, selector in self._leaves[1] if selector.matches(path)), default=(0, ""))
        self._names[path] = best[1]
        return best[1]

    def target_of(self, path: str) -> str:
        """How a violation names an included path: its module, else the whole path when modules are declared, else its top-level directory."""
        name = self.name_of(path)
        if name or any(part.kind == "module" for part in self.parts):
            return name
        return path.split("/")[0]

    def _add(self, text: str, source: Selector, target: Selector, kind: str, because: str, message: Message, qualifier: str, declared: str) -> None:
        self.rules.append(Rule(text, source, target, kind, because, message, qualifier, declared))  # type: ignore[arg-type]

    def _forbid(self, source: Selections, target: Selections, because: str, unless: Selections, message: Message, declared: str) -> None:
        src, tgt, exempt = _selection(source, "forbid source"), _selection(target, "forbid target"), _selection(unless, "forbid unless")
        suffix = f" except {exempt.label}" if exempt.words else ""
        self._add(
            f"{src.label} !-> {tgt.label}{suffix}",
            compile_selector(list(src.words), {}, list(exempt.words)),
            compile_selector(list(tgt.words), {}),
            "forbid", because, message, exempt.label if exempt.words else "", declared,
        )

    def _only(self, target: Selections, sources: Selections, because: str, message: Message, declared: str) -> None:
        tgt, allowed = _selection(target, "only target"), _selection(sources, "only sources")
        self._add(
            f"{tgt.label} <-only- {allowed.label}",
            compile_selector(list(self.everything.words), {}, list(allowed.words)),
            compile_selector(list(tgt.words), {}),
            "only", because, message, allowed.label, declared,
        )

    def forbid(self, source: Selections, target: Selections, *, because: str = "", unless: Selections = (), message: Message = "") -> None:
        """`source` files must not include `target`, except from files in `unless`."""
        self._forbid(source, target, because, unless, message, _declared())

    def only(self, target: Selections, *, from_: Selections, because: str = "", message: Message = "") -> None:
        """Only files in `from_` may include `target`."""
        self._only(target, from_, because, message, _declared())

    def isolate(self, subjects: Selections, *, from_: Selections, because: str = "", unless: Selections = (), message: Message = "") -> None:
        """Every subject must not include anything in `from_`: one rule and one reason for the whole group."""
        self._forbid(subjects, from_, because, unless, message, _declared())

    def peers(self, *members: Selection, because: str = "", message: Message = "") -> None:
        """None of the members may include another."""
        declared = _declared()
        for source in members:
            for target in members:
                if source is not target:
                    self._forbid(source, target, because, (), message, declared)

    def above(self, top: Selections, *lower: Selection, because: str = "", message: Message = "") -> None:
        """`top` sits above everything in `lower`, which must therefore not include it."""
        self._forbid(lower, top, because, (), message, _declared())

    def layers(self, *layers: Selections, because: str = "", message: Message = "") -> None:
        """Each layer may include itself and the layers before it, never one after it."""
        chosen = [_selection(layer, "a layer") for layer in layers]
        text = " < ".join(layer.label for layer in chosen)
        selectors = [compile_selector(list(layer.words), {}) for layer in chosen]
        declared = _declared()
        for index, lower in enumerate(selectors):
            for higher in selectors[index + 1:]:
                self._add(text, lower, higher, "layers", because, message, "", declared)

    def waive(self, source: Selections, target: Selections, *, because: str, expires: date | str) -> None:
        """Park a known violation: files in `source` may include `target` until `expires` (a date or YYYY-MM-DD), after which it is an error."""
        src, tgt = _selection(source, "waive source"), _selection(target, "waive target")
        deadline = expires if isinstance(expires, date) else date.fromisoformat(expires)
        self.waivers.append(Waiver(compile_selector(list(src.words), {}), compile_selector(list(tgt.words), {}), f"{src.label} -> {tgt.label}", because, deadline, _declared()))

    def symbols(self, name: str, *namespaces: str, prefixes: Iterable[str] = ()) -> Symbols:
        """Symbols in built artifacts, by namespace (`"oryx::python"` covers everything inside it) and/or prefix (`prefixes=["Py", "_Py"]`)."""
        if not namespaces and not prefixes:
            raise SchemaError(f"boundaries: symbols '{name}' needs at least one namespace or prefix")
        return Symbols(name, tuple(namespaces), tuple(prefixes))

    def optional(self, option: str, selection: "Selections | Symbols", *, because: str = "") -> OptionalPart:
        """Declare that the build option `option` can switch `selection` off. Pass the result to `when_on` / `when_off`."""
        if not option:
            raise SchemaError("boundaries: optional() needs the name of a build option")
        part = OptionalPart(option, selection if isinstance(selection, Symbols) else _selection(selection, f"optional '{option}'"), because)
        if any(known.option == option for known in self.optionals):
            raise SchemaError(f"boundaries: '{option}' is declared optional twice")
        self.optionals.append(part)
        return part

    def library(self, path: str, *, tool: str = "nm", when_on: "Options | None" = None, when_off: "Options | None" = None) -> "Library":
        """Checks on built artifacts matching the glob `path`; the conditions apply to every rule on it unless a rule gives its own."""
        from .library import Library, reason_of, requires_of

        library = Library(self, path, tool, requires_of(when_on, when_off), reason_of(when_on, when_off))
        self.libraries.append(library)
        return library
