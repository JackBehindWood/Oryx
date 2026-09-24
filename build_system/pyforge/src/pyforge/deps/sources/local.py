from pathlib import Path

from ..resolve import DependencyError, ResolvedDependency


def _shown(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix() if path.is_relative_to(root) else str(path)


class LocalSource:
    """Files you put in place yourself; forge never fetches, updates or deletes them."""

    def fetch(self, root: Path, dep: ResolvedDependency) -> None:
        raise DependencyError(f"'{dep.name}' is a local dependency: put its files at {_shown(root, dep.dir)}")

    def update(self, root: Path, dep: ResolvedDependency, rev: str | None) -> None:
        raise DependencyError(f"'{dep.name}' is a local dependency: replace the files at {_shown(root, dep.dir)} yourself")

    def remove(self, root: Path, dep: ResolvedDependency) -> None:
        pass

    def pin(self, root: Path, dep: ResolvedDependency) -> str:
        return ""
