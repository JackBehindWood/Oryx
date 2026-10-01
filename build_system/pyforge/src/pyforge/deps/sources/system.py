import shlex
import subprocess
from pathlib import Path

from ..resolve import DependencyError, ResolvedDependency


def _pkg_config(flag: str, name: str) -> str | None:
    if name.startswith("-"):
        return None
    try:
        result = subprocess.run(["pkg-config", flag, name], capture_output=True, text=True, encoding="utf-8", errors="replace")
    except OSError:
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def _absolute(root: Path, text: str) -> Path:
    path = Path(text)
    return path if path.is_absolute() else root / path


class SystemSource:
    """A library installed on the machine: found through pkg-config, or given as explicit include/lib paths."""

    def probe(self, root: Path, dep: ResolvedDependency) -> tuple[list[str], list[str]]:
        """(cflags, libs) for the compiler; raises DependencyError when the library can't be found."""
        spec = dep.spec
        if spec.pkg_config and _pkg_config("--exists", spec.pkg_config) is not None:
            return shlex.split(_pkg_config("--cflags", spec.pkg_config) or ""), shlex.split(_pkg_config("--libs", spec.pkg_config) or "")
        cflags: list[str] = []
        libs: list[str] = []
        if spec.include:
            include = _absolute(root, spec.include)
            if include.is_dir():
                cflags = [f"-I{include.as_posix()}"]
        if spec.lib:
            lib = _absolute(root, spec.lib)
            if lib.exists():
                libs = [f"-L{lib.as_posix()}"] if lib.is_dir() else [lib.as_posix()]
        found = bool(cflags) and (bool(libs) or not spec.lib)
        if not spec.pkg_config and found:
            return cflags, libs
        raise DependencyError(self._not_found(dep))

    @staticmethod
    def _not_found(dep: ResolvedDependency) -> str:
        spec = dep.spec
        if spec.pkg_config:
            return f"'{dep.name}' not found via pkg-config ({spec.pkg_config}) and no [dependencies.{dep.name}] include/lib paths given; install it or add those paths"
        return f"'{dep.name}' is a system dependency with no pkg-config name, and its include/lib paths don't exist; install it or fix those paths"

    def fetch(self, root: Path, dep: ResolvedDependency) -> None:
        self.probe(root, dep)

    def update(self, root: Path, dep: ResolvedDependency, rev: str | None) -> None:
        self.probe(root, dep)

    def remove(self, root: Path, dep: ResolvedDependency) -> None:
        pass

    def pin(self, root: Path, dep: ResolvedDependency) -> str:
        return (_pkg_config("--modversion", dep.spec.pkg_config) or "") if dep.spec.pkg_config else ""
