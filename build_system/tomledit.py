import re

_BARE_KEY = re.compile(r"[A-Za-z0-9_-]+")
_ESCAPES = {'"': '\\"', "\\": "\\\\", "\b": "\\b", "\t": "\\t", "\n": "\\n", "\f": "\\f", "\r": "\\r"}


def _string(value: str) -> str:
    escaped = "".join(
        _ESCAPES.get(char) or (f"\\u{ord(char):04x}" if ord(char) < 0x20 or ord(char) == 0x7F else char)
        for char in value
    )
    return f'"{escaped}"'


def _key(key: str) -> str:
    return key if _BARE_KEY.fullmatch(key) else _string(key)


def _value(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, str):
        return _string(value)
    if isinstance(value, list):
        return "[" + ", ".join(_value(item) for item in value) + "]"
    raise TypeError(f"Cannot write {type(value).__name__} to TOML")


def _table(table: dict, path: list[str], blocks: list[str]) -> None:
    scalars = [f"{_key(key)} = {_value(value)}" for key, value in table.items() if not isinstance(value, dict)]
    if path and (scalars or not table):
        scalars.insert(0, "[" + ".".join(_key(part) for part in path) + "]")
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
