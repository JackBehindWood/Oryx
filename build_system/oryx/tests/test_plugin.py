from pathlib import Path

from pyforge.config import load_config

REAL_ROOT = Path(__file__).resolve().parents[3]


def test_committed_forge_toml_parses():
    cfg = load_config(REAL_ROOT / "forge.toml", "0.2.0")
    assert cfg.project.default_target in cfg.targets
    assert cfg.options["python"].default is True
    assert cfg.tool["oryx"]["stubs-dir"] == "OryxPython/stubs"
    assert cfg.plugins.paths == ["build_system/oryx"]


def test_slang_tool_dependency_is_pinned_per_platform():
    cfg = load_config(REAL_ROOT / "forge.toml", "0.2.0")
    slang = cfg.dependencies["slang"]
    assert slang.kind == "tool" and slang.requires == ["graphics"] and slang.binary == "bin/slangc"
    assert {"macos-aarch64", "linux-x86_64"} <= set(slang.platforms)
    assert all(len(archive.sha256) == 64 for archive in slang.platforms.values())


def test_shader_cook_is_skipped_without_graphics_or_on_dry_run():
    import importlib
    from types import SimpleNamespace

    plugin = importlib.import_module("oryx")
    plugin.cook_shaders(SimpleNamespace(options={"graphics": False}, dry_run=False))
    plugin.cook_shaders(SimpleNamespace(options={"graphics": True}, dry_run=True))
