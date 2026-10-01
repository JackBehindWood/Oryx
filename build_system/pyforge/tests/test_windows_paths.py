import json
import os

from pyforge import cache
from pyforge.premake.driver import render_forge_lua
from pyforge.utils import child_env, write_json, write_text_lf


def test_write_text_lf_never_emits_crlf(tmp_path, monkeypatch):
    monkeypatch.setattr("os.linesep", "\r\n")
    path = tmp_path / "f.txt"
    write_text_lf(path, "a\nb\n")
    assert path.read_bytes() == b"a\nb\n"


def test_write_json_uses_lf(tmp_path):
    path = tmp_path / "f.json"
    write_json(path, {"a": [1, 2]})
    assert b"\r" not in path.read_bytes()


def test_generated_forge_lua_has_lf_endings(project):
    assert b"\r" not in render_forge_lua(project).read_bytes()


def test_pin_manifest_has_lf_endings(tmp_path, monkeypatch):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path))
    cache.record_pin(tmp_path / "proj", "lib", "abc")
    raw = (cache.deps_root() / ".pins.json").read_bytes()
    assert b"\r" not in raw and json.loads(raw)


def test_child_env_prepends_with_the_platform_separator(tmp_path, monkeypatch):
    monkeypatch.setenv("PATH", "x")
    assert child_env(tmp_path)["PATH"] == f"{tmp_path}{os.pathsep}x"
