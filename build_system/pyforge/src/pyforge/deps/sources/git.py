import os
import re
import shutil
import tempfile
from pathlib import Path

from ... import cache
from ..resolve import DependencyError, ResolvedDependency
from .submodule import _git

_SHA = re.compile(r"[0-9a-f]{7,40}")


def _clone(url: str, ref: str, dest: Path) -> None:
    if _SHA.fullmatch(ref):
        dest.mkdir(parents=True)
        _git(dest, "init", "-q")
        _git(dest, "remote", "add", "origin", url)
        _git(dest, "fetch", "-q", "--depth", "1", "origin", ref)
        _git(dest, "checkout", "-q", "FETCH_HEAD")
    else:
        _git(dest.parent, "clone", "-q", "--depth", "1", "--branch", ref, url, str(dest))


def default_branch(url: str) -> str:
    output = _git(Path.cwd(), "ls-remote", "--symref", url, "HEAD")
    match = re.search(r"ref: refs/heads/(\S+)\s+HEAD", output)
    if not match:
        raise DependencyError(f"Could not find the default branch of {url}; pass --rev.")
    return match.group(1)


def install_clone(root: Path, name: str, url: str, ref: str) -> str:
    """Clone `ref` into the shared cache under its resolved commit; returns that commit."""
    scratch = Path(tempfile.mkdtemp(prefix=f"{name}.clone-", dir=_scratch_parent()))
    try:
        clone = scratch / "repo"
        _clone(url, ref, clone)
        commit = _git(clone, "rev-parse", "HEAD")
        cache.atomic_extract(cache.deps_dir(name, commit), lambda tmp: os.replace(clone, tmp))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    cache.record_pin(root, name, commit)
    return commit


def _scratch_parent() -> Path:
    parent = cache.deps_root() / ".tmp"
    parent.mkdir(parents=True, exist_ok=True)
    return parent


class GitSource:
    """A shallow git clone in the shared cache, keyed by the commit forge.toml pins."""

    def fetch(self, root: Path, dep: ResolvedDependency) -> None:
        spec = dep.spec
        scratch = Path(tempfile.mkdtemp(prefix=f"{dep.name}.clone-", dir=_scratch_parent()))
        try:
            clone = scratch / "repo"
            try:
                _clone(spec.url, spec.commit, clone)
            except DependencyError:
                shutil.rmtree(clone, ignore_errors=True)
                _clone(spec.url, spec.rev, clone)
            actual = _git(clone, "rev-parse", "HEAD")
            if actual != spec.commit:
                raise DependencyError(f"{dep.name}: '{spec.rev}' resolves to {actual} but forge.toml pins commit {spec.commit}. Run: forge deps update {dep.name}")
            cache.atomic_extract(dep.dir, lambda tmp: os.replace(clone, tmp))
        finally:
            shutil.rmtree(scratch, ignore_errors=True)
        cache.record_pin(root, dep.name, spec.commit)

    def update(self, root: Path, dep: ResolvedDependency, rev: str | None) -> str:
        """Clones the new `rev` (default: the remote's default branch) and returns its commit for forge.toml."""
        return install_clone(root, dep.name, dep.spec.url, rev or default_branch(dep.spec.url))

    def remove(self, root: Path, dep: ResolvedDependency) -> None:
        pass

    def pin(self, root: Path, dep: ResolvedDependency) -> str:
        if not dep.present:
            return ""
        actual = _git(dep.dir, "rev-parse", "HEAD")
        if actual != dep.spec.commit:
            raise DependencyError(f"Cache entry {dep.dir} is at {actual}, not the pinned {dep.spec.commit}; run: forge deps clean-cache")
        return actual[:12]
