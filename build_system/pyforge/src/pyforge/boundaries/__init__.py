"""Optional layering checks: a plugin (`forge_boundaries`) or `[boundaries]` in forge.toml says what may depend on what; `forge boundaries check` enforces it."""

from .artifacts import register_artifact_tool
from .check import Boundaries, Violation
from .rules import Include, Rule, parse_rules
from .scanners import register_scanner
from .select import Selector, compile_selector
from .library import Library
from .optional import OptionalPart
from .spec import BoundarySpec, Selection
from .symbols import Symbols
from .suite import BoundarySuite, specs_of

__all__ = ["Boundaries", "BoundarySpec", "BoundarySuite", "Include", "Library", "OptionalPart", "Selection", "Symbols", "specs_of", "Rule", "Selector", "Violation", "compile_selector", "parse_rules", "register_artifact_tool", "register_scanner"]
