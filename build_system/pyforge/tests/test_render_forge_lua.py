from pyforge.premake.driver import FORGE_LUA_HEADER, LUA_FRAGMENTS, render_forge_lua
from pyforge.project import Project


def _fragments(tmp_path):
    lua_dir = tmp_path / "lua"
    lua_dir.mkdir()
    for name in LUA_FRAGMENTS:
        (lua_dir / f"{name}.lua").write_text(f"local x = '{name}'\n", encoding="utf-8")
    return lua_dir


def _project(tmp_path):
    root = tmp_path / "proj"
    root.mkdir()
    return Project(root=root, config_file=root / "forge.toml")


def test_bundle_wraps_each_fragment_in_do_end_in_fixed_order(tmp_path):
    path = render_forge_lua(_project(tmp_path), _fragments(tmp_path))
    text = path.read_text(encoding="utf-8")
    assert text.startswith(FORGE_LUA_HEADER)
    positions = [text.index(f"local x = '{name}'") for name in LUA_FRAGMENTS]
    assert positions == sorted(positions)
    assert text.count("do\n") == len(LUA_FRAGMENTS) and text.count("\nend\n") == len(LUA_FRAGMENTS)


def test_bundle_creates_missing_premake_directory(tmp_path):
    project = _project(tmp_path)
    assert render_forge_lua(project, _fragments(tmp_path)) == project.root / "premake" / "forge.lua"


def test_bundle_is_only_rewritten_when_content_changes(tmp_path):
    project, lua_dir = _project(tmp_path), _fragments(tmp_path)
    path = render_forge_lua(project, lua_dir)
    before = path.stat().st_mtime_ns
    render_forge_lua(project, lua_dir)
    assert path.stat().st_mtime_ns == before
    (lua_dir / "toolchain.lua").write_text("local x = 'changed'\n", encoding="utf-8")
    render_forge_lua(project, lua_dir)
    assert "changed" in path.read_text(encoding="utf-8")
