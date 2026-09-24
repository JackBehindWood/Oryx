from dataclasses import dataclass
from pathlib import Path

CONFIG_NAMES = ("forge.toml", "oryx.toml")


class ProjectNotFound(RuntimeError):
    pass


def find_root(start: Path | None = None) -> Path:
    """The nearest directory at or above `start` holding a forge config file."""
    current = (start or Path.cwd()).resolve()
    for directory in (current, *current.parents):
        if any((directory / name).is_file() for name in CONFIG_NAMES):
            return directory
    raise ProjectNotFound(f"No forge.toml found in {current} or any parent directory. Run `forge init` to create one.")


def _config_file(root: Path) -> Path:
    return next((root / name for name in CONFIG_NAMES if (root / name).is_file()), root / CONFIG_NAMES[0])


@dataclass(frozen=True)
class Project:
    root: Path
    config_file: Path

    @classmethod
    def discover(cls, start: Path | None = None) -> "Project":
        root = find_root(start)
        return cls(root=root, config_file=_config_file(root))

    @classmethod
    def from_config(cls, config_file: Path) -> "Project":
        config_file = config_file.resolve()
        return cls(root=config_file.parent, config_file=config_file)

    def path(self, relative: str | Path) -> Path:
        return self.root / relative

    @property
    def build_dir(self) -> Path:
        return self.root / "build"

    @property
    def forge_dir(self) -> Path:
        return self.build_dir / "forge"

    @property
    def bin_dir(self) -> Path:
        return self.build_dir / "bin"

    @property
    def premake_bin_dir(self) -> Path:
        return self.root / "premake" / "bin"
