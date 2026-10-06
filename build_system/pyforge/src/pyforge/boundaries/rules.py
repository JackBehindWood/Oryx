import re
from dataclasses import dataclass

from ..config.schema import BoundariesTable, SchemaError
from .select import Selector, compile_selector

_DENY = re.compile(r"^(?P<source>.+?)\s*!->\s*(?P<target>.+?)(?:\s+except\s+(?P<allowed>.+))?$")
_ONLY = re.compile(r"^(?P<target>.+?)\s*<-only-\s*(?P<allowed>.+)$")


@dataclass(frozen=True)
class Include:
    file: str
    raw: str
    path: str

    @property
    def module(self) -> str:
        return self.file.split("/")[0]

    @property
    def target(self) -> str:
        return self.path.split("/")[0]


@dataclass(frozen=True)
class Rule:
    text: str
    source: Selector
    target: Selector
    why: str

    def violation(self, include: Include) -> str | None:
        if self.source.matches(include.file) and self.target.matches(include.path):
            return self.why.format(module=include.module, target=include.target, file=include.file, path=include.path)
        return None


def _words(text: str) -> list[str]:
    return text.split()


def _parse(text: str, why: str, sets: dict[str, list[str]], where: str) -> list[Rule]:
    def rule(source: Selector, target: Selector, default: str) -> Rule:
        return Rule(text, source, target, why or default)

    if match := _DENY.match(text):
        source = compile_selector(_words(match["source"]), sets, _words(match["allowed"] or ""))
        return [rule(source, compile_selector(_words(match["target"]), sets), "{module} must not include {target}")]
    if match := _ONLY.match(text):
        source = compile_selector(["**"], sets, _words(match["allowed"]))
        return [rule(source, compile_selector(_words(match["target"]), sets), f"only {match['allowed']} may include {{target}}")]
    if "<" in text:
        layers = [compile_selector(_words(layer), sets) for layer in text.split("<")]
        return [
            rule(lower, higher, "{module} must not include the higher layer {target}")
            for index, lower in enumerate(layers)
            for higher in layers[index + 1:]
        ]
    raise SchemaError(f"forge.toml: {where} {text!r} must look like 'A !-> B [except C]', 'A <-only- B' or 'A < B < C'")


def parse_rules(table: BoundariesTable) -> list[Rule]:
    return [rule for index, entry in enumerate(table.rules) for rule in _parse(entry.rule, entry.why, table.sets, f"boundaries.rules[{index}]")]
