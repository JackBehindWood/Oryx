import tempfile
from pathlib import Path
from urllib.parse import urlparse

from ... import cache
from ..resolve import DependencyError, ResolvedDependency


def download_verified(url: str, sha256: str, destination: Path, description: str) -> None:
    from ...utils import download_with_progress, sha256_file

    try:
        download_with_progress(url, destination, description)
    except (OSError, ValueError) as error:
        raise DependencyError(f"Could not download {url}: {error}") from error
    actual = sha256_file(destination)
    if actual != sha256:
        destination.unlink(missing_ok=True)
        raise DependencyError(f"sha256 mismatch for {url}: expected {sha256}, got {actual}. Update 'sha256' in forge.toml if the archive legitimately changed.")


def url_filename(url: str) -> str:
    name = Path(urlparse(url).path).name
    return name if name not in ("", ".", "..") else "download"


def _flatten(directory: Path) -> None:
    children = list(directory.iterdir())
    if len(children) == 1 and children[0].is_dir():
        inner = children[0]
        staging = directory.with_name(directory.name + ".flat")
        inner.rename(staging)
        directory.rmdir()
        staging.rename(directory)


class ArchiveSource:
    """A .tar.gz/.zip downloaded into the shared cache, keyed by its sha256; a single top-level folder is flattened."""

    def fetch(self, root: Path, dep: ResolvedDependency) -> None:
        if not dep.spec.url:
            from ..resolve import host_platform

            raise DependencyError(f"Dependency '{dep.name}' has no archive for {host_platform()}; forge.toml lists: {', '.join(sorted(dep.spec.platforms)) or 'none'}")
        with tempfile.TemporaryDirectory(prefix="forge-dl-") as scratch:
            archive = Path(scratch) / url_filename(dep.spec.url)
            download_verified(dep.spec.url, dep.spec.sha256, archive, dep.name)
            self.install(root, dep, archive)

    def install(self, root: Path, dep: ResolvedDependency, archive: Path) -> None:
        import tarfile
        import zipfile

        from ...utils import extract_archive

        def extract(tmp: Path) -> None:
            try:
                extract_archive(archive, tmp)
            except (ValueError, OSError, tarfile.TarError, zipfile.BadZipFile) as error:
                raise DependencyError(f"Could not extract {dep.spec.url}: {error}") from error
            _flatten(tmp)

        cache.atomic_extract(dep.dir, extract)
        cache.record_pin(root, dep.name, self.pin(root, dep))

    def update(self, root: Path, dep: ResolvedDependency, rev: str | None) -> None:
        self.fetch(root, dep)

    def remove(self, root: Path, dep: ResolvedDependency) -> None:
        pass

    def pin(self, root: Path, dep: ResolvedDependency) -> str:
        return dep.spec.sha256[:12]
