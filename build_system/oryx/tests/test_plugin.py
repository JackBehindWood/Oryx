from pathlib import Path

from pyforge.config import load_config

REAL_ROOT = Path(__file__).resolve().parents[3]


def test_committed_forge_toml_parses():
    cfg = load_config(REAL_ROOT / "forge.toml", "0.2.0")
    assert cfg.project.default_target in cfg.targets
    assert cfg.options["python"].default is True
    assert cfg.tool["oryx"]["stubs-dir"] == "OryxPython/stubs"
    assert cfg.plugins.paths == ["build_system/oryx"]
