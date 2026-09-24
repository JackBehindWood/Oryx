import os
import re
import tempfile
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

_BARE_KEY = re.compile(r"[A-Za-z0-9_-]+")
_ESCAPES = {'"': '\\"', "\\": "\\\\", "\b": "\\b", "\t": "\\t", "\n": "\\n", "\f": "\\f", "\r": "\\r"}
_KEY_PART = re.compile(r"""\s*("(?:[^"\\]|\\.)*"|'[^']*'|[A-Za-z0-9_-]+)\s*""")
_HEADER = re.compile(r"^\s*(\[\[?)(.*?)(\]\]?)\s*(#.*)?$")


class TomlEditError(ValueError):
    pass


def _string(value: str) -> str:
    escaped = "".join(
        _ESCAPES.get(char) or (f"\\u{ord(char):04x}" if ord(char) < 0x20 or ord(char) == 0x7F else char)
        for char in value
    )
    return f'"{escaped}"'


def format_key(key: str) -> str:
    return key if _BARE_KEY.fullmatch(key) else _string(key)


def format_value(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, str):
        return _string(value)
    if isinstance(value, list):
        return "[" + ", ".join(format_value(item) for item in value) + "]"
    if isinstance(value, dict):
        return "{ " + ", ".join(f"{format_key(key)} = {format_value(item)}" for key, item in value.items()) + " }" if value else "{}"
    raise TypeError(f"Cannot write {type(value).__name__} to TOML")


def _table(table: dict, path: list[str], blocks: list[str]) -> None:
    scalars = [f"{format_key(key)} = {format_value(value)}" for key, value in table.items() if not isinstance(value, dict)]
    if path and (scalars or not table):
        scalars.insert(0, "[" + ".".join(format_key(part) for part in path) + "]")
    if scalars:
        blocks.append("\n".join(scalars))
    for key, value in table.items():
        if isinstance(value, dict):
            _table(value, path + [key], blocks)


def dumps(data: dict) -> str:
    """Serialize nested dicts of str/bool/int/float/list values to TOML text that `tomllib` reads back unchanged."""
    blocks: list[str] = []
    _table(data, [], blocks)
    return "\n\n".join(blocks) + "\n"


def _unquote(part: str) -> str:
    if part.startswith('"'):
        return tomllib.loads(f"k = {part}")["k"]
    if part.startswith("'"):
        return part[1:-1]
    return part


def _split_dotted(text: str) -> list[str] | None:
    parts, position = [], 0
    while True:
        match = _KEY_PART.match(text, position)
        if not match:
            return None
        parts.append(_unquote(match.group(1)))
        position = match.end()
        if position == len(text):
            return parts
        if text[position] != ".":
            return None
        position += 1


@dataclass
class _Section:
    path: tuple[str, ...]
    header: int
    end: int
    array: bool = False


def _sections(lines: list[str]) -> list[_Section]:
    sections = [_Section(path=(), header=-1, end=len(lines))]
    for index, line in enumerate(lines):
        match = _HEADER.match(line)
        if not match or line.lstrip().startswith("#"):
            continue
        path = _split_dotted(match.group(2))
        if path is None:
            continue
        sections[-1].end = index
        sections.append(_Section(path=tuple(path), header=index, end=len(lines), array=match.group(1) == "[["))
    return sections


def _key_line(line: str) -> tuple[list[str], str, str] | None:
    """(dotted key, text up to and including '= ' , rest) for a `key = value` line."""
    stripped = line.lstrip()
    if not stripped or stripped.startswith(("#", "[")):
        return None
    position, depth = 0, None
    for index, char in enumerate(line):
        if char in "\"'" and depth is None:
            depth = char
        elif char == depth:
            depth = None
        elif char == "=" and depth is None:
            position = index
            break
    else:
        return None
    key = _split_dotted(line[:position].strip())
    if key is None:
        return None
    after = position + 1
    while after < len(line) and line[after] in " \t":
        after += 1
    return key, line[:after], line[after:]


def _split_value(rest: str, where: str) -> tuple[str, str]:
    """Split the text after '=' into (value, trailing whitespace + comment), refusing multi-line values."""
    candidates = [index for index, char in enumerate(rest) if char == "#"] + [len(rest)]
    for index in candidates:
        value = rest[:index].rstrip()
        try:
            tomllib.loads(f"k = {value}")
        except tomllib.TOMLDecodeError:
            continue
        return value, rest[len(value):]
    raise TomlEditError(f"'{where}' spans several lines; edit it by hand")


def _dotted(path: tuple[str, ...] | list[str]) -> str:
    return ".".join(path)


def _locate(lines: list[str], path: list[str]):
    """The section holding `path`'s key (or table), and the key line index when present."""
    sections = _sections(lines)
    full = tuple(path)
    for section in sections:
        if section.path == full:
            if section.array:
                raise TomlEditError(f"'{_dotted(path)}' is an array of tables; edit it by hand")
            return section, None, True
    owner = None
    for section in sections:
        if section.path == full[:-1] and not section.array:
            owner = section
    if owner is None:
        for depth in range(len(full) - 2, -1, -1):
            candidates = [section for section in sections if section.path == full[:depth]]
            for section in candidates:
                if _find_key(lines, section, full[depth]) is not None:
                    raise TomlEditError(f"'{_dotted(full[:depth + 1])}' is an inline table or dotted key; edit '{_dotted(path)}' by hand")
        return None, None, False
    return owner, _find_key(lines, owner, full[-1]), False


def _find_key(lines: list[str], section: _Section, key: str) -> int | None:
    for index in range(section.header + 1, section.end):
        parsed = _key_line(lines[index])
        if parsed is None:
            continue
        dotted = parsed[0]
        if dotted == [key]:
            return index
        if dotted[0] == key:
            raise TomlEditError(f"'{_dotted([*section.path, key])}' is written as dotted keys; edit it by hand")
    return None


def _last_content_line(lines: list[str], section: _Section) -> int:
    for index in range(section.end - 1, section.header, -1):
        stripped = lines[index].strip()
        if stripped and not stripped.startswith("#"):
            return index
    return section.header


def _join(lines: list[str]) -> str:
    return "\n".join(lines) + "\n" if lines else ""


def set_value(text: str, path: list[str], value: Any) -> str:
    """Set `path` (e.g. ["build", "jobs"]) to `value`, touching only that line, or appending it to its table."""
    if not path:
        raise TomlEditError("empty key path")
    lines = text.splitlines()
    section, index, is_table = _locate(lines, path)
    if is_table:
        raise TomlEditError(f"'{_dotted(path)}' is a table; set one of its keys instead")
    line = f"{format_key(path[-1])} = {format_value(value)}"
    if section is None:
        return _append_table(lines, path[:-1], line)
    if index is None:
        lines.insert(_last_content_line(lines, section) + 1, line)
        return _join(lines)
    _, prefix, rest = _key_line(lines[index])
    _, trailer = _split_value(rest, _dotted(path))
    lines[index] = prefix + format_value(value) + trailer
    return _join(lines)


def _append_table(lines: list[str], table: list[str], line: str) -> str:
    if not table:
        sections = _sections(lines)
        lines.insert(_last_content_line(lines, sections[0]) + 1, line)
        return _join(lines)
    sections = _sections(lines)
    siblings = [section for section in sections if section.path[: len(table) - 1] == tuple(table[:-1]) and section.path]
    position = siblings[-1].end if siblings and len(table) > 1 else len(lines)
    while position > 0 and not lines[position - 1].strip():
        position -= 1
    block = ([""] if position > 0 else []) + ["[" + ".".join(format_key(part) for part in table) + "]", line]
    lines[position:position] = block
    return _join(lines)


def unset(text: str, path: list[str]) -> str:
    """Remove the `path` key line, or the whole `[path]` table."""
    lines = text.splitlines()
    section, index, is_table = _locate(lines, path)
    if is_table:
        del lines[section.header:section.end]
        while lines and not lines[-1].strip():
            lines.pop()
        return _join(lines)
    if section is None or index is None:
        raise TomlEditError(f"'{_dotted(path)}' is not set")
    _split_value(_key_line(lines[index])[2], _dotted(path))
    del lines[index]
    return _join(lines)


def append(text: str, path: list[str], item: Any) -> str:
    """Append `item` to the single-line array at `path`, creating the array when the key is absent."""
    lines = text.splitlines()
    section, index, is_table = _locate(lines, path)
    if is_table:
        raise TomlEditError(f"'{_dotted(path)}' is a table, not an array")
    current = []
    if section is not None and index is not None:
        value, _ = _split_value(_key_line(lines[index])[2], _dotted(path))
        current = tomllib.loads(f"k = {value}")["k"]
        if not isinstance(current, list):
            raise TomlEditError(f"'{_dotted(path)}' is not an array")
    return set_value(text, path, [*current, item])


def edit_file(path: Path, edit: Callable[[str], str], validate: Callable[[dict], Any] | None = None) -> str:
    """Apply `edit` to the file's text, re-parse and validate the result, then replace the file atomically."""
    original = path.read_text(encoding="utf-8") if path.is_file() else ""
    updated = edit(original)
    try:
        data = tomllib.loads(updated)
    except tomllib.TOMLDecodeError as error:
        raise TomlEditError(f"{path.name}: the edit would produce invalid TOML ({error}); nothing was written") from error
    if validate is not None:
        validate(data)
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.", suffix=".tmp")
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="") as file:
            file.write(updated)
        os.replace(temporary, path)
    except BaseException:
        Path(temporary).unlink(missing_ok=True)
        raise
    return updated
