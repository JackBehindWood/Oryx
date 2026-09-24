import subprocess
from pathlib import Path

from ...utils import run_command
from ..resolve import DependencyError, ResolvedDependency


def _relative(root: Path, dep: ResolvedDependency) -> str:
    return dep.dir.relative_to(root).as_posix()


def _git(root: Path, *args: str) -> str:
    try:
        return run_command(["git", *args], cwd=root).stdout.strip()
    except FileNotFoundError as error:
        raise DependencyError("git was not found on PATH") from error
    except subprocess.CalledProcessError as error:
        raise DependencyError(f"`git {' '.join(args)}` failed:\n{(error.stderr or error.stdout or '').strip()}") from error


class SubmoduleSource:
    """A git submodule at the dependency's path; the superproject records the pinned commit."""

    def add(self, root: Path, dep: ResolvedDependency, url: str) -> None:
        _git(root, "submodule", "add", url, _relative(root, dep))

    def fetch(self, root: Path, dep: ResolvedDependency) -> None:
        _git(root, "submodule", "update", "--init", "--recursive", "--", _relative(root, dep))

    def update(self, root: Path, dep: ResolvedDependency, rev: str | None) -> None:
        path = _relative(root, dep)
        if rev is None:
            _git(root, "submodule", "update", "--init", "--remote", "--recursive", "--", path)
            return
        _git(root, "-C", path, "fetch", "--tags", "origin")
        _git(root, "-C", path, "checkout", rev)

    def remove(self, root: Path, dep: ResolvedDependency) -> None:
        path = _relative(root, dep)
        _git(root, "submodule", "deinit", "-f", "--", path)
        _git(root, "rm", "-f", "--", path)

    def pin(self, root: Path, dep: ResolvedDependency) -> str:
        if not dep.present:
            return ""
        try:
            return _git(root, "-C", _relative(root, dep), "rev-parse", "--short", "HEAD")
        except DependencyError:
            return ""
