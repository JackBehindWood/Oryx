import tomllib

import pytest

from pyforge.tomledit import TomlEditError, append, dumps, edit_file, set_value, unset


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


FORGE = """\
# Project config
[project]
name = "Oryx"  # the engine

[build]
jobs = 0                         # 0 = all cores
fetch = "auto"

[options]
python = { default = true, off = "--no-python" }

[targets.oasis]
project = "Oasis"
presets.bench = ["--a", "--b"]

[dependencies]
spdlog = { kind = "static", include = "include" }
defines = [
    "A",
]
"""


def _lines_changed(before: str, after: str) -> list[str]:
    old, new = before.splitlines(), after.splitlines()
    return [line for line in new if line not in old] + [f"-{line}" for line in old if line not in new]


def test_set_replaces_only_the_value_and_keeps_the_comment():
    after = set_value(FORGE, ["build", "jobs"], 4)
    assert _lines_changed(FORGE, after) == ["jobs = 4                         # 0 = all cores", "-jobs = 0                         # 0 = all cores"]
    assert tomllib.loads(after)["build"]["jobs"] == 4


def test_set_value_containing_a_hash():
    after = set_value(FORGE, ["project", "name"], "C# #1")
    assert 'name = "C# #1"  # the engine' in after
    assert tomllib.loads(set_value(after, ["project", "name"], "x"))["project"]["name"] == "x"


def test_set_new_key_goes_after_the_last_key_of_its_table():
    after = set_value(FORGE, ["build", "default-profile"], "release")
    assert 'fetch = "auto"\ndefault-profile = "release"\n\n[options]' in after


def test_set_inline_table_value():
    after = set_value(FORGE, ["dependencies", "glad"], {"source": "local", "kind": "static", "defines": ["X"]})
    assert 'glad = { source = "local", kind = "static", defines = ["X"] }' in after
    assert tomllib.loads(after)["dependencies"]["glad"]["defines"] == ["X"]


def test_set_creates_a_missing_table_at_the_end():
    after = set_value(FORGE, ["docs", "tool"], "mkdocs")
    assert after.endswith(']\n\n[docs]\ntool = "mkdocs"\n')


def test_set_creates_a_sibling_subtable_next_to_its_family():
    after = set_value(FORGE, ["targets", "bench", "project"], "Bench")
    assert '[targets.oasis]\nproject = "Oasis"\npresets.bench = ["--a", "--b"]\n\n[targets.bench]\nproject = "Bench"\n\n[dependencies]' in after


def test_set_root_key_in_an_empty_file():
    assert set_value("", ["name"], "x") == 'name = "x"\n'
    assert set_value("", ["editor", "kind"], "vscode") == '[editor]\nkind = "vscode"\n'


def test_quoted_keys_round_trip():
    after = set_value(FORGE, ["dependencies", "yaml-cpp"], {"kind": "static"})
    after = set_value(after, ["dependencies", "a.b"], True)
    assert '"a.b" = true' in after
    assert tomllib.loads(after)["dependencies"]["a.b"] is True
    assert tomllib.loads(set_value(after, ["dependencies", "a.b"], False))["dependencies"]["a.b"] is False


@pytest.mark.parametrize(
    ("path", "message"),
    [
        (["dependencies", "defines"], "spans several lines"),
        (["options", "python", "default"], "'options.python' is an inline table or dotted key"),
        (["targets", "oasis", "presets", "extra"], "is written as dotted keys"),
        (["build"], "is a table"),
    ],
)
def test_unsupported_edits_are_refused(path, message):
    with pytest.raises(TomlEditError, match=message):
        set_value(FORGE, path, 1)


def test_array_of_tables_is_refused():
    with pytest.raises(TomlEditError, match="array of tables"):
        set_value("[[bin]]\nname = 'a'\n", ["bin"], 1)


def test_unset_key_line():
    after = unset(FORGE, ["build", "fetch"])
    assert _lines_changed(FORGE, after) == ['-fetch = "auto"']


def test_unset_whole_table():
    after = unset(FORGE, ["options"])
    assert "[options]" not in after and "python" not in after
    assert '[build]\njobs = 0                         # 0 = all cores\nfetch = "auto"\n\n[targets.oasis]' in after


def test_unset_last_table_trims_trailing_blank_lines():
    assert unset('[a]\nx = 1\n\n[b]\ny = 2\n', ["b"]) == "[a]\nx = 1\n"


def test_unset_missing_key():
    with pytest.raises(TomlEditError, match="'build.nope' is not set"):
        unset(FORGE, ["build", "nope"])


def test_append_to_single_line_array_and_create_missing():
    after = append('[t]\nitems = ["a"]  # keep\n', ["t", "items"], "b")
    assert after == '[t]\nitems = ["a", "b"]  # keep\n'
    assert append("[t]\n", ["t", "items"], "a") == '[t]\nitems = ["a"]\n'


def test_append_refuses_non_arrays():
    with pytest.raises(TomlEditError, match="not an array"):
        append(FORGE, ["build", "jobs"], 1)


def test_edit_file_writes_atomically_and_validates(tmp_path):
    path = tmp_path / "forge.toml"
    path.write_text(FORGE, encoding="utf-8")
    edit_file(path, lambda text: set_value(text, ["build", "jobs"], 2))
    assert tomllib.loads(path.read_text(encoding="utf-8"))["build"]["jobs"] == 2
    assert [p.name for p in tmp_path.iterdir()] == ["forge.toml"]


def test_edit_file_leaves_the_file_alone_when_validation_fails(tmp_path):
    path = tmp_path / "forge.toml"
    path.write_text(FORGE, encoding="utf-8")

    def reject(data):
        raise ValueError("schema says no")

    with pytest.raises(ValueError, match="schema says no"):
        edit_file(path, lambda text: set_value(text, ["build", "jobs"], "many"), validate=reject)
    assert path.read_text(encoding="utf-8") == FORGE
    assert [p.name for p in tmp_path.iterdir()] == ["forge.toml"]


def test_edit_file_refuses_invalid_toml(tmp_path):
    path = tmp_path / "forge.toml"
    path.write_text(FORGE, encoding="utf-8")
    with pytest.raises(TomlEditError, match="invalid TOML"):
        edit_file(path, lambda text: text + "[broken\n")
    assert path.read_text(encoding="utf-8") == FORGE


def test_edit_file_creates_a_missing_file(tmp_path):
    path = tmp_path / "forge.local.toml"
    edit_file(path, lambda text: set_value(text, ["editor", "kind"], "vscode"))
    assert path.read_text(encoding="utf-8") == '[editor]\nkind = "vscode"\n'


def test_dumps_writes_dict_values_inside_arrays_as_inline_tables():
    assert tomllib.loads(dumps({"t": {"x": [{"a": 1}]}})) == {"t": {"x": [{"a": 1}]}}


def test_editor_vscode_keeps_comments_in_forge_local_toml(forge, tmp_project):
    local = tmp_project / "forge.local.toml"
    local.write_text('# mine\n[editor]\nkind = "none"  # default\n\n[build]\njobs = 2\n', encoding="utf-8")
    assert forge("editor", "vscode", "--debugger", "cppdbg").exit_code == 0
    assert local.read_text(encoding="utf-8") == '# mine\n[editor]\nkind = "vscode"  # default\ndebugger = "cppdbg"\n\n[build]\njobs = 2\n'
