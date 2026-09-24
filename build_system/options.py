"""Build options: forge.toml [options] → values → Premake flags, and the hash that decides an output wipe."""

import hashlib
import re

from .config import ForgeConfig, LocalConfig, OptionSpec, SchemaError
from .config.schema import _suggestion

_DEFINE = re.compile(r"([A-Za-z0-9][A-Za-z0-9_-]*)(?:=(.*))?", re.DOTALL)


def _check_name(name: str, specs: dict[str, OptionSpec], flag: str) -> None:
    if name not in specs:
        available = ", ".join(specs) or "(none in forge.toml)"
        raise SchemaError(f"command line: {flag} {name}: unknown option{_suggestion(name, specs)} (available: {available})")


def resolve(cfg: ForgeConfig, local: LocalConfig, with_: list[str] = (), without: list[str] = ()) -> dict[str, bool]:
    """Option values: forge.toml defaults, then forge.local.toml, then --with/--without."""
    values = {name: spec.default for name, spec in cfg.options.items()}
    values.update(local.options)
    for name in with_:
        _check_name(name, cfg.options, "--with")
        values[name] = True
    for name in without:
        _check_name(name, cfg.options, "--without")
        values[name] = False
    return values


def define_flag(define: str) -> str:
    match = _DEFINE.fullmatch(define)
    if not match:
        raise SchemaError(f"command line: -D {define!r}: expected KEY or KEY=VALUE")
    key, value = match.groups()
    return f"--{key}" if value is None else f"--{key}={value}"


def premake_flags(specs: dict[str, OptionSpec], values: dict[str, bool], defines: list[str] = ()) -> list[str]:
    """Each option's `on` or `off` flag for its current value, then `-D` defines as raw Premake options."""
    flags = [flag for name, spec in specs.items() if (flag := spec.on if values[name] else spec.off)]
    return flags + [define_flag(define) for define in defines]


def options_hash(flags: list[str]) -> str:
    return hashlib.sha256("\0".join(flags).encode("utf-8")).hexdigest()
