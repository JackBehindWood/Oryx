import tomllib

import pytest

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
