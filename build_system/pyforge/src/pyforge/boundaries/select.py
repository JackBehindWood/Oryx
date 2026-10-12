import re
from collections.abc import Sequence
from dataclasses import dataclass

from ..config.schema import SchemaError


def _split_alternatives(body: str) -> list[str]:
    parts, depth, start = [], 0, 0
    for index, char in enumerate(body):
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
        elif char == "," and depth == 0:
            parts.append(body[start:index])
            start = index + 1
    return parts + [body[start:]]


def _glob_regex(glob: str) -> str:
    parts, index = [], 0
    while index < len(glob):
        if glob.startswith("**/", index):
            parts.append("(?:.*/)?")
            index += 3
        elif glob.startswith("**", index):
            parts.append(".*")
            index += 2
        elif glob[index] == "*":
            parts.append("[^/]*")
            index += 1
        elif glob[index] == "?":
            parts.append("[^/]")
            index += 1
        elif glob[index] == "{":
            close = _closing_brace(glob, index)
            parts.append("(?:" + "|".join(_glob_regex(option) for option in _split_alternatives(glob[index + 1:close])) + ")")
            index = close + 1
        else:
            parts.append(re.escape(glob[index]))
            index += 1
    return "".join(parts)


def _closing_brace(glob: str, start: int) -> int:
    depth = 0
    for index in range(start, len(glob)):
        depth += {"{": 1, "}": -1}.get(glob[index], 0)
        if depth == 0:
            return index
    raise SchemaError(f"boundaries: unbalanced '{{' in glob {glob!r}")


@dataclass(frozen=True)
class Token:
    """A bare word names a module or directory, `re:` starts a regex, anything with * ? or {a,b} is a glob; a glob without a `/` matches a file name at any depth."""

    text: str
    pattern: re.Pattern
    anywhere: bool = False

    def matches(self, path: str) -> bool:
        return (self.pattern.search if self.anywhere else self.pattern.match)(path) is not None


def _token(text: str) -> Token:
    if text.startswith("re:"):
        return Token(text, re.compile(text[3:]), anywhere=True)
    if any(char in text for char in "*?{"):
        anywhere_prefix = "" if "/" in text else "(?:.*/)?"
        return Token(text, re.compile(anywhere_prefix + _glob_regex(text) + r"\Z"))
    return Token(text, re.compile(re.escape(text) + r"(?:/.*)?\Z"))


@dataclass(frozen=True)
class Selector:
    """Paths matched by any positive token and no negative one; no positive tokens means everything."""

    include: tuple[Token, ...]
    exclude: tuple[Token, ...] = ()

    def matches(self, path: str) -> bool:
        return (not self.include or any(token.matches(path) for token in self.include)) and not any(token.matches(path) for token in self.exclude)


def _expand(word: str, sets: dict[str, list[str]], seen: tuple[str, ...] = ()) -> list[str]:
    if word not in sets:
        return [word]
    if word in seen:
        raise SchemaError(f"forge.toml: boundaries.sets has a cycle through '{word}'")
    return [leaf for member in sets[word] for leaf in _expand(member, sets, (*seen, word))]


def compile_selector(words: Sequence[str], sets: dict[str, list[str]], exclude: Sequence[str] = ()) -> Selector:
    """`words` may name entries of `sets`, which expand recursively; a leading `!` negates a word."""
    include = [leaf for word in words if not word.startswith("!") for leaf in _expand(word, sets)]
    negated = [word.removeprefix("!") for word in words if word.startswith("!")] + list(exclude)
    excluded = [leaf for word in negated for leaf in _expand(word, sets)]
    return Selector(tuple(map(_token, include)), tuple(map(_token, excluded)))
