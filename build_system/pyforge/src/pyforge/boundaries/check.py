import re
from dataclasses import dataclass
from pathlib import Path

from ..config.schema import BoundariesTable, suggestion
from .artifacts import ARTIFACT_SCANNERS
from .rules import Include, Rule, parse_rules
from .scanners import SCANNERS


@dataclass(frozen=True)
class Violation:
    subject: str
    message: str

    def __str__(self) -> str:
        return f"{self.subject}: {self.message}"


class Boundaries:
    def __init__(self, table: BoundariesTable):
        self.table = table
        self.rules: list[Rule] = parse_rules(table)

    def _logical(self, raw: str) -> str:
        base = self.table.base
        return raw.removeprefix(base + "/") if base and raw.startswith(base + "/") else raw

    def includes(self, project_root: Path) -> list[Include]:
        scanner = SCANNERS[self.table.scan]
        root = project_root / self.table.root
        tree = root / self.table.base
        found = []
        for path in sorted(tree.rglob("*")):
            if path.suffix in scanner.suffixes:
                file = path.relative_to(tree).as_posix()
                text = path.read_text(encoding="utf-8", errors="replace")
                found += [Include(file, raw, self._logical(raw)) for raw in scanner.imports(text)]
        return found

    def check(self, project_root: Path) -> list[Violation]:
        """One violation per include: the first rule it breaks, in declaration order."""
        root = project_root / self.table.root
        tree = root / self.table.base
        violations = []
        for include in self.includes(project_root):
            for rule in self.rules:
                if message := rule.violation(include):
                    where = (tree / include.file).relative_to(root).as_posix()
                    violations.append(Violation(where, f"includes {include.raw} ({message})"))
                    break
        return violations

    def unmatched(self, project_root: Path) -> list[str]:
        """Selector words that match no file and no include — usually a typo for a set name."""
        includes = self.includes(project_root)
        paths = {include.file for include in includes} | {include.path for include in includes}
        tree = project_root / self.table.root / self.table.base
        paths |= {path.relative_to(tree).as_posix() for path in tree.rglob("*") if path.is_file()}
        dead = []
        for rule in self.rules:
            for token in (*rule.source.include, *rule.target.include):
                if token.text not in dead and not any(token.matches(path) for path in paths):
                    dead.append(token.text)
        return [f"'{word}' matches nothing{suggestion(word, self.table.sets)}" for word in dead]

    def check_artifacts(self, project_root: Path, options: dict[str, bool]) -> list[Violation]:
        from ..deps.resolve import requirements_met

        violations = []
        for artifact in self.table.artifacts:
            if not requirements_met(artifact.requires, options):
                continue
            matches = sorted(project_root.glob(artifact.path))
            if not matches:
                violations.append(Violation(artifact.path, "no artifact found; build first"))
            pattern = re.compile(artifact.deny)
            for path in matches:
                seen = sorted({line.strip() for line in ARTIFACT_SCANNERS[artifact.tool](path) if pattern.search(line)})
                reason = f" ({artifact.why})" if artifact.why else ""
                violations += [Violation(path.relative_to(project_root).as_posix(), f"exports {line}{reason}") for line in seen]
        return violations
