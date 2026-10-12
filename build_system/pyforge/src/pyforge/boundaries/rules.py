import re
from collections.abc import Callable
from dataclasses import dataclass
from typing import Literal, TypeAlias

from ..config.schema import BoundariesTable, SchemaError
from .select import Selector, compile_selector

_DENY = re.compile(r"^(?P<source>.+?)\s*!->\s*(?P<target>.+?)(?:\s+except\s+(?P<allowed>.+))?$")
_ONLY = re.compile(r"^(?P<target>.+?)\s*<-only-\s*(?P<allowed>.+)$")

RuleKind: TypeAlias = Literal["forbid", "only", "layers"]


class _Known(dict):
    """Template values where an unknown {name} stays as written instead of crashing the whole check."""

    def __missing__(self, key: str) -> str:
        return "{" + key + "}"


@dataclass(frozen=True)
class Include:
    file: str
    raw: str
    path: str
    line: int = 0

    @property
    def module(self) -> str:
        return self.file.split("/")[0]

    @property
    def target(self) -> str:
        return self.path.split("/")[0]


Namer: TypeAlias = Callable[[str], str]
MessageFn: TypeAlias = Callable[[Include, "Rule"], str]
Message: TypeAlias = str | MessageFn


@dataclass(frozen=True)
class Rule:
    """One include restriction. `message` replaces the generated sentence; `{default}` inside it extends it."""

    text: str
    source: Selector
    target: Selector
    kind: RuleKind = "forbid"
    because: str = ""
    message: Message = ""
    qualifier: str = ""
    declared: str = ""

    def sentence(self, source: str, target: str) -> str:
        if self.kind == "only":
            body = f"{target} may only be included by {self.qualifier}"
            notes = [self.because]
        elif self.kind == "layers":
            body = f"{source} must not include {target}, a higher layer"
            notes = [self.because]
        else:
            body = f"{source} must not include {target}"
            notes = [f"except {self.qualifier}" if self.qualifier else "", self.because]
        reasons = "; ".join(note for note in notes if note)
        return f"{body} ({reasons})" if reasons else body

    def violation(self, include: Include, name_of: Namer, fallback: Message = "", target_of: Namer | None = None) -> str | None:
        if not (self.source.matches(include.file) and self.target.matches(include.path)):
            return None
        source = name_of(include.file) or include.module
        target = (target_of or name_of)(include.path) or include.path
        template = self.message or fallback or "{default}"
        if callable(template):
            return template(include, self)
        values = _Known(
            default=self.sentence(source, target), because=self.because, source=source, module=source, target=target,
            file=include.file, line=include.line, path=include.path, raw=include.raw, rule=self.text,
        )
        return template.format_map(values)


def _words(text: str) -> list[str]:
    return text.split()


def _parse(text: str, because: str, message: str, sets: dict[str, list[str]], where: str) -> list[Rule]:
    if match := _DENY.match(text):
        source = compile_selector(_words(match["source"]), sets, _words(match["allowed"] or ""))
        target = compile_selector(_words(match["target"]), sets)
        return [Rule(text, source, target, "forbid", because, message, match["allowed"] or "")]
    if match := _ONLY.match(text):
        source = compile_selector(["**"], sets, _words(match["allowed"]))
        return [Rule(text, source, compile_selector(_words(match["target"]), sets), "only", because, message, match["allowed"])]
    if "<" in text:
        layers = [compile_selector(_words(layer), sets) for layer in text.split("<")]
        return [
            Rule(text, lower, higher, "layers", because, message)
            for index, lower in enumerate(layers)
            for higher in layers[index + 1:]
        ]
    raise SchemaError(f"forge.toml: {where} {text!r} must look like 'A !-> B [except C]', 'A <-only- B' or 'A < B < C'")


def parse_rules(table: BoundariesTable) -> list[Rule]:
    return [
        rule
        for index, entry in enumerate(table.rules)
        for rule in _parse(entry.rule, entry.because, entry.message or entry.why, table.sets, f"boundaries.rules[{index}]")
    ]
