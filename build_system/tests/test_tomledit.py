import tomllib

import pytest

from build_system.config import BuildConfig, LocalConfig
from build_system.tests.conftest import REAL_ROOT
from build_system.tomledit import dumps


@pytest.mark.parametrize(
    "text",
    [
        'say "hi"',
        "C:\\Users\\dev\\oryx",
        "ünïcödé — 名前 🎲",
        "line one\nline two\r\n",
        "tab\tbell\x07escape\x1bdelete\x7f",
        "",
    ],
)
def test_string_round_trip(text):
    data = {"value": text, "table": {"value": text}}
    assert tomllib.loads(dumps(data)) == data


def test_mixed_values_and_nested_tables_round_trip():
    data = {
        "flag": False,
        "count": -3,
        "ratio": 0.25,
        "names": ["a", 'b"c', "d\\e"],
        "outer": {"inner": {"deep": {"x": 1}}, "y": True},
        "needs quoting": {"key.with.dots": "v", "": "empty key"},
        "empty": {},
    }
    assert tomllib.loads(dumps(data)) == data


def test_parent_table_without_scalars_has_no_header():
    assert dumps({"executables": {"oasis": {"name": "Oasis"}}}) == '[executables.oasis]\nname = "Oasis"\n'


def test_unsupported_value_raises():
    with pytest.raises(TypeError):
        dumps({"value": object()})


def test_build_config_save_matches_committed_file(tmp_path):
    path = tmp_path / "oryx.toml"
    BuildConfig().save(path)
    assert path.read_text(encoding="utf-8") == (REAL_ROOT / "oryx.toml").read_text(encoding="utf-8")


def test_build_config_round_trip_escapes(tmp_path):
    path = tmp_path / "oryx.toml"
    config = BuildConfig(project_name='My "Game"\\Engine', python_enabled=False)
    config.save(path)
    loaded = BuildConfig.load(path)
    assert loaded.project_name == config.project_name
    assert loaded.python_enabled is False
    assert loaded.executables == config.executables


def test_local_config_round_trip(tmp_path):
    path = tmp_path / "oryx.local.toml"
    LocalConfig(ide_kind="vscode", debugger="cppdbg").save(path)
    assert LocalConfig.load(path) == LocalConfig(ide_kind="vscode", debugger="cppdbg")
