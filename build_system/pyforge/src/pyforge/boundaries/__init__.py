"""Optional, declarative layering checks: `[boundaries]` in forge.toml says what may depend on what; `forge boundaries check` enforces it."""

from .artifacts import register_artifact_tool
from .check import Boundaries, Violation
from .rules import Include, Rule, parse_rules
from .scanners import register_scanner
from .select import Selector, compile_selector

__all__ = ["Boundaries", "Include", "Rule", "Selector", "Violation", "compile_selector", "parse_rules", "register_artifact_tool", "register_scanner"]
