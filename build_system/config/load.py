import re
import tomllib
from pathlib import Path

from .schema import ForgeConfig, LocalConfig, SchemaError, _suggestion, from_dict

LOCAL_CONFIG_NAME = "forge.local.toml"
LEGACY_LOCAL_CONFIG_NAME = "oryx.local.toml"


def _read(path: Path) -> dict:
    try:
        with path.open("rb") as file:
            return tomllib.load(file)
    except tomllib.TOMLDecodeError as error:
        raise SchemaError(f"{path.name}: {error}") from error


def _version_tuple(text: str) -> tuple[int, ...]:
    return tuple(int(part) for part in re.findall(r"\d+", text)[:3])


def check_forge_version(requirement: str, installed: str) -> None:
    if not requirement:
        return
    match = re.fullmatch(r"\s*>=\s*([\d.]+)\s*", requirement)
    if not match:
        raise SchemaError(f"forge.toml: 'project.forge-version' must look like '>=0.2', not {requirement!r}")
    if _version_tuple(installed) < _version_tuple(match.group(1)):
        raise SchemaError(f"This project needs forge {requirement} but {installed} is installed. Upgrade forge (e.g. `uv sync`).")


def _requirement_name(requirement: str) -> str:
    return requirement.removeprefix("!")


def validate(cfg: ForgeConfig) -> ForgeConfig:
    """Cross-table checks that a single table's schema can't express."""
    options = cfg.options.keys()
    for name, dependency in cfg.dependencies.items():
        for requirement in dependency.requires:
            option = _requirement_name(requirement)
            if option not in options:
                raise SchemaError(f"forge.toml: 'dependencies.{name}.requires' names unknown option '{option}'{_suggestion(option, options)}")
    default_target = cfg.project.default_target
    if default_target and default_target not in cfg.targets:
        raise SchemaError(
            f"forge.toml: 'project.default-target' is '{default_target}', which is not in [targets]{_suggestion(default_target, cfg.targets)}"
        )
    return cfg


def parse_config(data: dict) -> ForgeConfig:
    return validate(from_dict(ForgeConfig, data))


def load_config(path: Path, installed_version: str) -> ForgeConfig:
    cfg = parse_config(_read(path))
    check_forge_version(cfg.project.forge_version, installed_version)
    return cfg


def _legacy_to_local(data: dict) -> dict:
    data = dict(data)
    if "ide" in data:
        data["editor"] = data.pop("ide")
    return data


def local_config_file(root: Path) -> Path:
    return root / LOCAL_CONFIG_NAME


def load_local(root: Path) -> tuple[LocalConfig, Path | None]:
    """The per-developer config, and the legacy file it came from when only oryx.local.toml exists."""
    path = local_config_file(root)
    if path.is_file():
        return from_dict(LocalConfig, _read(path), source=LOCAL_CONFIG_NAME), None
    legacy = root / LEGACY_LOCAL_CONFIG_NAME
    if legacy.is_file():
        return from_dict(LocalConfig, _legacy_to_local(_read(legacy)), source=LEGACY_LOCAL_CONFIG_NAME), legacy
    return LocalConfig(), None


def validate_local(local: LocalConfig, cfg: ForgeConfig) -> LocalConfig:
    for name in local.options:
        if name not in cfg.options:
            raise SchemaError(f"{LOCAL_CONFIG_NAME}: unknown option '{name}' in [options]{_suggestion(name, cfg.options)}")
    return local


def local_data_for_save(root: Path) -> dict:
    """The raw table a local-preference save starts from: forge.local.toml, else the legacy file's content."""
    path = local_config_file(root)
    if path.is_file():
        return _read(path)
    legacy = root / LEGACY_LOCAL_CONFIG_NAME
    return _legacy_to_local(_read(legacy)) if legacy.is_file() else {}
