import re
from dataclasses import dataclass, field


@dataclass(frozen=True)
class Symbols:
    """A set of symbols in built artifacts, named by namespace (`oryx::python`, which covers everything inside it) or by prefix (`Py`).

    Declare with `spec.symbols("python", "oryx::python", "pybind11", prefixes=["Py", "_Py"])` and pass to a library's
    `must_not_export` / `must_not_import`, or to `spec.optional`.
    """

    label: str
    namespaces: tuple[str, ...] = ()
    prefixes: tuple[str, ...] = ()
    _patterns: tuple[tuple[str, re.Pattern[str]], ...] = field(default=(), compare=False, repr=False)

    def __post_init__(self) -> None:
        patterns = [(name, re.compile(rf"(?<![\w:]){re.escape(name)}::")) for name in self.namespaces]
        patterns += [(prefix, re.compile(rf"(?<![\w:]){re.escape(prefix)}\w")) for prefix in self.prefixes]
        object.__setattr__(self, "_patterns", tuple(patterns))

    def __str__(self) -> str:
        return self.label

    def find(self, symbol: str) -> str:
        """The namespace or prefix that claims `symbol`, or '' when none does."""
        return next((name for name, pattern in self._patterns if pattern.search(symbol)), "")
