from dataclasses import dataclass
from typing import TYPE_CHECKING, TypeAlias, Iterable

if TYPE_CHECKING:
    from .spec import Selection
    from .symbols import Symbols

@dataclass(frozen=True)
class OptionalPart:
    """A part of the tree that a build option can switch off. Pass it to `when_on` / `when_off`, which then use its option name and,
    for a rule without a `because` of its own, its reason."""

    option: str
    selection: "Selection | Symbols"
    because: str = ""

    def __str__(self) -> str:
        return self.option

Options: TypeAlias = str | OptionalPart | Iterable[str | OptionalPart]
