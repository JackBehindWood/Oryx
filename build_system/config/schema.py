import dataclasses
import difflib
import types
import typing
from dataclasses import dataclass, field
from enum import Enum, StrEnum
from typing import Any


class Profile(StrEnum):
    DEBUG = "debug"
    RELEASE = "release"
    DIST = "dist"


class FetchMode(StrEnum):
    AUTO = "auto"
    ASK = "ask"
    NEVER = "never"


class DependencyKind(StrEnum):
    STATIC = "static"
    HEADER = "header"


class Debugger(StrEnum):
    LLDB = "lldb"
    CPPDBG = "cppdbg"


class Choices:
    """An open set of names that plugins extend with register(), unlike the closed enums above."""

    def __init__(self, *names: str):
        self._names = list(names)

    def register(self, name: str) -> None:
        if name not in self._names:
            self._names.append(name)

    def __contains__(self, name: object) -> bool:
        return name in self._names

    def __iter__(self):
        return iter(self._names)


GENERATORS = Choices("gmake")
DEPENDENCY_SOURCES = Choices("submodule", "local")
EDITORS = Choices("vscode", "visual_studio", "none")
DOCS_TOOLS = Choices("mkdocs")


def _open(choices: Choices, default: str):
    return field(default=default, metadata={"choices": choices})


class SchemaError(ValueError):
    pass


@dataclass(frozen=True)
class ProjectTable:
    name: str
    description: str = ""
    license: str = ""
    repository: str = ""
    default_target: str = ""
    forge_version: str = ""


@dataclass(frozen=True)
class PremakeTable:
    version: str = "5.0.0-beta8"
    generator: str = _open(GENERATORS, "gmake")


@dataclass(frozen=True)
class BuildTable:
    default_profile: Profile = Profile.DEBUG
    jobs: int = 0
    dependencies_dir: str = "vendor"
    fetch: FetchMode = FetchMode.AUTO


@dataclass(frozen=True)
class OptionSpec:
    default: bool = False
    on: str = ""
    off: str = ""
    help: str = ""


@dataclass(frozen=True)
class Target:
    project: str
    presets: dict[str, list[str]] = field(default_factory=dict)


@dataclass(frozen=True)
class TestsTable:
    project: str


@dataclass(frozen=True)
class Dependency:
    kind: DependencyKind = DependencyKind.HEADER
    source: str = _open(DEPENDENCY_SOURCES, "submodule")
    path: str = ""
    include: str = ""
    sources: str = ""
    defines: list[str] = field(default_factory=list)
    requires: list[str] = field(default_factory=list)


@dataclass(frozen=True)
class DocsTable:
    tool: str = _open(DOCS_TOOLS, "mkdocs")
    config: str = "mkdocs.yml"
    site: str = "site"


@dataclass(frozen=True)
class ForgeConfig:
    project: ProjectTable
    premake: PremakeTable = field(default_factory=PremakeTable)
    build: BuildTable = field(default_factory=BuildTable)
    options: dict[str, OptionSpec] = field(default_factory=dict)
    targets: dict[str, Target] = field(default_factory=dict)
    tests: TestsTable | None = None
    dependencies: dict[str, Dependency] = field(default_factory=dict)
    docs: DocsTable | None = None
    tool: dict[str, dict] = field(default_factory=dict)


@dataclass(frozen=True)
class EditorTable:
    kind: str = _open(EDITORS, "none")
    debugger: Debugger = Debugger.LLDB


@dataclass(frozen=True)
class LocalBuildTable:
    default_profile: Profile | None = None
    jobs: int | None = None
    fetch: FetchMode | None = None


@dataclass(frozen=True)
class LocalConfig:
    editor: EditorTable = field(default_factory=EditorTable)
    build: LocalBuildTable = field(default_factory=LocalBuildTable)
    options: dict[str, bool] = field(default_factory=dict)


def toml_key(name: str) -> str:
    return name.replace("_", "-")


def _join(where: str, key: str) -> str:
    return f"{where}.{key}" if where else key


def _suggestion(key: str, candidates: typing.Iterable[str]) -> str:
    matches = difflib.get_close_matches(key, list(candidates), n=1)
    return f" — did you mean '{matches[0]}'?" if matches else ""


def _type_name(annotation) -> str:
    return {str: "a string", int: "an integer", bool: "a boolean"}.get(annotation, getattr(annotation, "__name__", str(annotation)))


def _check_choice(value, choices, where: str, source: str) -> None:
    choices = list(choices)
    if value not in choices:
        allowed = ", ".join(repr(choice) for choice in choices)
        hint = _suggestion(value, choices) if isinstance(value, str) else ""
        raise SchemaError(f"{source}: '{where}' is {value!r}; expected one of {allowed}{hint}")


def parse_enum(enum: type[Enum], value, where: str, source: str = "forge.toml"):
    _check_choice(value, (member.value for member in enum), where, source)
    return enum(value)


def _convert(annotation, value, where: str, source: str):
    origin = typing.get_origin(annotation)
    if origin is types.UnionType or origin is typing.Union:
        inner = next(arg for arg in typing.get_args(annotation) if arg is not type(None))
        return _convert(inner, value, where, source)
    if isinstance(annotation, type) and issubclass(annotation, Enum):
        return parse_enum(annotation, value, where, source)
    if dataclasses.is_dataclass(annotation):
        if not isinstance(value, dict):
            raise SchemaError(f"{source}: '{where}' must be a table")
        return from_dict(annotation, value, where, source)
    if origin is dict:
        _, value_type = typing.get_args(annotation)
        if not isinstance(value, dict):
            raise SchemaError(f"{source}: '{where}' must be a table")
        return {key: _convert(value_type, item, _join(where, key), source) for key, item in value.items()}
    if origin is list:
        (item_type,) = typing.get_args(annotation)
        if not isinstance(value, list):
            raise SchemaError(f"{source}: '{where}' must be an array")
        return [_convert(item_type, item, f"{where}[{index}]", source) for index, item in enumerate(value)]
    if annotation is int and isinstance(value, bool) or not isinstance(value, annotation):
        raise SchemaError(f"{source}: '{where}' must be {_type_name(annotation)}, not {type(value).__name__}")
    return value


def from_dict(cls, data: dict, where: str = "", source: str = "forge.toml"):
    """Build dataclass `cls` from a parsed TOML table, rejecting unknown or mistyped keys."""
    fields = {toml_key(f.name): f for f in dataclasses.fields(cls)}
    hints = typing.get_type_hints(cls)

    for key in data:
        if key not in fields:
            raise SchemaError(f"{source}: unknown key '{_join(where, key)}'{_suggestion(key, fields)}")

    values: dict[str, Any] = {}
    for key, f in fields.items():
        if key not in data:
            if f.default is dataclasses.MISSING and f.default_factory is dataclasses.MISSING:
                raise SchemaError(f"{source}: missing required key '{_join(where, key)}'")
            continue
        value = _convert(hints[f.name], data[key], _join(where, key), source)
        if "choices" in f.metadata:
            _check_choice(value, f.metadata["choices"], _join(where, key), source)
        values[f.name] = value
    return cls(**values)
