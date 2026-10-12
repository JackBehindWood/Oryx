from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

from ..config.schema import BoundariesTable, SchemaError, suggestion
from .library import check_library
from .rules import Include, Rule
from .scanners import SCANNERS
from .select import Selector, compile_selector
from .spec import BoundarySpec
from .violation import Violation

__all__ = ["Boundaries", "Scan", "Violation"]


def _line_of(text: str, raw: str) -> int:
    index = text.find(raw)
    return text.count("\n", 0, index) + 1 if index >= 0 else 0


@dataclass
class Scan:
    """Everything one pass over the source tree finds: includes, the names each file declares, and every file path."""

    includes: list[Include] = field(default_factory=list)
    declarations: dict[str, set[str]] = field(default_factory=dict)
    files: set[str] = field(default_factory=set)
    can_declare: bool = False

    def exclusive_to(self, selector: Selector) -> set[str]:
        """Names declared only in files the selector matches (a name also declared elsewhere belongs to neither side)."""
        if not self.can_declare:
            raise SchemaError("boundaries: this language's scanner cannot list declarations; register it with `declares=` to use must_not_export / must_export")
        return {name for name, files in self.declarations.items() if all(selector.matches(file) for file in files)}


class Boundaries:
    def __init__(self, source: BoundarySpec | BoundariesTable, today: Callable[[], date] = date.today) -> None:
        self.spec = source if isinstance(source, BoundarySpec) else BoundarySpec.from_table(source)
        self.rules: list[Rule] = self.spec.rules
        self.waived: list[Violation] = []
        self._today = today
        self._scans: dict[Path, Scan] = {}
        self._used_waivers: set[int] = set()

    def _logical(self, raw: str) -> str:
        base = self.spec.base
        return raw.removeprefix(base + "/") if base and raw.startswith(base + "/") else raw

    def scan(self, project_root: Path) -> Scan:
        if project_root in self._scans:
            return self._scans[project_root]
        scanner = SCANNERS[self.spec.scan]
        tree = project_root / self.spec.root / self.spec.base
        result = Scan(can_declare=scanner.declares is not None)
        for path in sorted(tree.rglob("*")):
            if not path.is_file():
                continue
            file = path.relative_to(tree).as_posix()
            result.files.add(file)
            if path.suffix not in scanner.suffixes:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            result.includes += [Include(file, raw, self._logical(scanner.resolve(raw)), _line_of(text, raw)) for raw in scanner.imports(text)]
            if scanner.declares is not None:
                for name in scanner.declares(text):
                    result.declarations.setdefault(name, set()).add(file)
        self._scans[project_root] = result
        return result

    def includes(self, project_root: Path) -> list[Include]:
        return self.scan(project_root).includes

    def check(self, project_root: Path) -> list[Violation]:
        """One violation per include: the first rule it breaks, in declaration order. Waived ones go to `self.waived`."""
        root = project_root / self.spec.root
        tree = root / self.spec.base
        today = self._today()
        violations: list[Violation] = []
        self.waived = []
        self._used_waivers = set()
        for include in self.scan(project_root).includes:
            for rule in self.rules:
                message = rule.violation(include, self.spec.name_of, self.spec.message, self.spec.target_of)
                if message is None:
                    continue
                where = (tree / include.file).relative_to(root).as_posix()
                found = (tree / include.file).relative_to(project_root).as_posix()
                waiver_index = next((i for i, w in enumerate(self.spec.waivers) if w.source.matches(include.file) and w.target.matches(include.path)), None)
                if waiver_index is not None:
                    waiver = self.spec.waivers[waiver_index]
                    self._used_waivers.add(waiver_index)
                    if today <= waiver.expires:
                        self.waived.append(Violation(where, f"{message} (waived until {waiver.expires}: {waiver.because})", found, include.line, rule.text, self.spec.name))
                        break
                    message = f"{message} (waiver expired {waiver.expires}: {waiver.because})"
                violations.append(Violation(where, message, found, include.line, rule.text, self.spec.name))
                break
        return violations

    def stale_waivers(self, project_root: Path) -> list[Violation]:
        """Waivers that suppress nothing: the violation they parked is gone, so the exception should be too."""
        self.check(project_root)
        return [
            Violation(waiver.label, f"waiver matches no violation; remove it ({waiver.because})", "", 0, waiver.label, self.spec.name)
            for index, waiver in enumerate(self.spec.waivers)
            if index not in self._used_waivers
        ]

    def dead_parts(self, project_root: Path) -> list[Violation]:
        """Declared modules, file sets and patterns that match no file and no include: almost always a misspelt path."""
        paths = self._known_paths(project_root)
        selectors = {part.label: compile_selector(list(part.words), {}) for part in self.spec.parts if part.required}
        return [
            Violation(label, "matches no file and no include; fix its path or remove it", "", 0, label, self.spec.name)
            for label, selector in selectors.items()
            if not any(selector.matches(path) for path in paths)
        ]

    def _known_paths(self, project_root: Path) -> set[str]:
        scan = self.scan(project_root)
        return scan.files | {include.file for include in scan.includes} | {include.path for include in scan.includes}

    def unmatched(self, project_root: Path) -> list[str]:
        """Selector words that match no file and no include, usually a typo for a set name; guards declared `allow_empty` are expected to match nothing."""
        paths = self._known_paths(project_root)
        optional = {word for part in self.spec.parts if not part.required and part.kind != "group" for word in part.words}
        dead = []
        for rule in self.rules:
            for token in (*rule.source.include, *rule.target.include):
                if token.text not in dead and token.text not in optional and not any(token.matches(path) for path in paths):
                    dead.append(token.text)
        return [f"'{word}' matches nothing{suggestion(word, self._candidates(word))}" for word in dead]

    def _candidates(self, word: str) -> dict[str, list[str]]:
        """Set names worth suggesting for a word: not the ones that already contain it."""
        return {label: words for label, words in self.spec.sets.items() if word not in words}

    def check_artifacts(self, project_root: Path, options: dict[str, bool]) -> list[Violation]:
        scan = self.scan(project_root)
        return [violation for library in self.spec.libraries for violation in check_library(library, project_root, options, scan)]
