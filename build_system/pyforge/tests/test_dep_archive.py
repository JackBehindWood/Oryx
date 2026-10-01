import functools
import hashlib
import http.server
import io
import tarfile
import threading
import zipfile

import pytest

from pyforge import cache
from pyforge.config import Dependency, SchemaError, parse_config
from pyforge.deps.resolve import DependencyError, ResolvedDependency
from pyforge.deps.sources.archive import ArchiveSource
from pyforge.utils import extract_archive


@pytest.fixture
def server(tmp_path):
    root = tmp_path / "www"
    root.mkdir()
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(root))
    handler.log_message = lambda *args: None
    httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    yield root, f"http://127.0.0.1:{httpd.server_port}"
    httpd.shutdown()


def _tarball(path, members):
    with tarfile.open(path, "w:gz") as tar:
        for name, data in members.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            tar.addfile(info, io.BytesIO(data))


def _dep(url, sha, name="lib"):
    spec = Dependency(source="archive", url=url, sha256=sha)
    return ResolvedDependency(name, spec, cache.deps_dir(name, sha[:12]))


def test_archive_round_trip_flattens_top_level_folder(server, tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    root, base = server
    _tarball(root / "lib.tar.gz", {"lib-1.0/include/lib.h": b"// h"})
    sha = hashlib.sha256((root / "lib.tar.gz").read_bytes()).hexdigest()
    dep = _dep(f"{base}/lib.tar.gz", sha)
    ArchiveSource().fetch(tmp_path, dep)
    assert (dep.dir / "include" / "lib.h").read_text() == "// h"
    assert dep.present
    assert cache.read_pins()[str(tmp_path)]["lib"] == sha[:12]


def test_wrong_sha_leaves_no_cache_entry(server, tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    root, base = server
    _tarball(root / "lib.tar.gz", {"a.h": b"x"})
    dep = _dep(f"{base}/lib.tar.gz", "0" * 64)
    with pytest.raises(DependencyError, match="sha256 mismatch"):
        ArchiveSource().fetch(tmp_path, dep)
    assert not cache.deps_root().exists() or not any(cache.deps_root().rglob("*"))


def test_traversal_members_rejected(tmp_path):
    bad_zip = tmp_path / "bad.zip"
    with zipfile.ZipFile(bad_zip, "w") as zf:
        zf.writestr("../../evil.txt", "x")
    with pytest.raises(ValueError, match="Unsafe"):
        extract_archive(bad_zip, tmp_path / "out")
    bad_tar = tmp_path / "bad.tar.gz"
    _tarball(bad_tar, {"../../evil.txt": b"x"})
    with pytest.raises(tarfile.TarError):
        extract_archive(bad_tar, tmp_path / "out2")
    assert not (tmp_path.parent / "evil.txt").exists()


def test_archive_requires_url_and_sha():
    with pytest.raises(SchemaError, match="url"):
        parse_config({"project": {"name": "p"}, "dependencies": {"x": {"source": "archive", "url": "http://h/x.zip"}}})


def test_file_source_lands_unextracted(server, tmp_path, monkeypatch):
    from pyforge.deps.sources.file import FileSource

    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    root, base = server
    (root / "stb.h").write_bytes(b"// stb")
    sha = hashlib.sha256(b"// stb").hexdigest()
    spec = Dependency(source="file", url=f"{base}/stb.h", sha256=sha)
    dep = ResolvedDependency("stb", spec, cache.deps_dir("stb", sha[:12]))
    FileSource().fetch(tmp_path, dep)
    assert [p.name for p in dep.dir.iterdir()] == ["stb.h"]
    assert (dep.dir / "stb.h").read_bytes() == b"// stb"


def test_cli_add_file_writes_url_and_sha(server, tmp_project, tmp_path, monkeypatch, forge):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    root, base = server
    (root / "stb.h").write_bytes(b"// stb")
    result = forge("deps", "add", "stb", "--file", f"{base}/stb.h")
    assert result.exit_code == 0, result.output
    text = (tmp_project / "forge.toml").read_text()
    assert 'source = "file"' in text and hashlib.sha256(b"// stb").hexdigest() in text
