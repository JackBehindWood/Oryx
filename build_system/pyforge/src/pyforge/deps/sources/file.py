import shutil
import tempfile
from pathlib import Path

from ... import cache
from ..resolve import ResolvedDependency
from .archive import ArchiveSource, download_verified, url_filename


class FileSource(ArchiveSource):
    """A single file (e.g. a header) downloaded as-is into the shared cache, keyed by its sha256."""

    def fetch(self, root: Path, dep: ResolvedDependency) -> None:
        with tempfile.TemporaryDirectory(prefix="forge-dl-") as scratch:
            download = Path(scratch) / url_filename(dep.spec.url)
            download_verified(dep.spec.url, dep.spec.sha256, download, dep.name)
            self.install(root, dep, download)

    def install(self, root: Path, dep: ResolvedDependency, archive: Path) -> None:
        def place(tmp: Path) -> None:
            tmp.mkdir(parents=True)
            shutil.copy2(archive, tmp / url_filename(dep.spec.url))

        cache.atomic_extract(dep.dir, place)
        cache.record_pin(root, dep.name, self.pin(root, dep))
