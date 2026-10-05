import pytest

from pyforge.boundaries import Boundaries, compile_selector, register_scanner
from pyforge.config import SchemaError, parse_config


def _boundaries(rules, sets=None, **table) -> Boundaries:
    cfg = parse_config({"project": {"name": "p"}, "boundaries": {"rules": rules, "sets": sets or {}, **table}})
    return Boundaries(cfg.boundaries)


def _tree(tmp_path, files):
    for relative, content in files.items():
        path = tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    return tmp_path


def test_selector_words():
    sets = {"ui": ["Gui", "Widgets"], "vendor": ["re:^third_"]}
    assert compile_selector(["ui"], sets).matches("Gui/Button.h")
    assert not compile_selector(["ui"], sets).matches("Guix/Button.h")
    assert compile_selector(["**/gen/**"], sets).matches("a/b/gen/c.h")
    assert compile_selector(["vendor"], sets).matches("third_party/x.h")
    assert compile_selector(["**", "!Gui"], sets).matches("Core/a.h")
    assert not compile_selector(["**", "!Gui"], sets).matches("Gui/a.h")


def test_set_cycle_is_rejected():
    with pytest.raises(SchemaError, match="cycle"):
        compile_selector(["a"], {"a": ["b"], "b": ["a"]})


def test_deny_with_except(tmp_path):
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n', "A/ok.h": '#include "B/y.h"\n'})
    assert len(_boundaries(["A !-> B except A/ok*"]).check(src)) == 1


def test_only_allows_listed_files(tmp_path):
    src = _tree(tmp_path, {"A/x.h": '#include "Secret.h"\n', "Core/f.cpp": '#include "Secret.h"\n'})
    violations = _boundaries(["Secret.h <-only- Core/f.cpp"]).check(src)
    assert [v.subject for v in violations] == ["A/x.h"]


def test_layers_forbid_every_upward_include(tmp_path):
    src = _tree(tmp_path, {"L1/a.h": '#include "L3/c.h"\n', "L2/b.h": '#include "L1/a.h"\n', "L3/c.h": '#include "L1/a.h"\n'})
    assert [v.subject for v in _boundaries(["L1 < L2 < L3"]).check(src)] == ["L1/a.h"]


def test_base_prefix_is_stripped_and_first_rule_wins(tmp_path):
    src = _tree(tmp_path, {"src/App/Game/g.h": '#include "App/Render/r.h"\n'})
    boundaries = _boundaries([{"rule": "Game !-> Render", "why": "first"}, "Game !-> Render"], root="src", base="App")
    assert [str(v) for v in boundaries.check(src)] == ["App/Game/g.h: includes App/Render/r.h (first)"]


def test_unmatched_selector_suggests_set(tmp_path):
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n'})
    assert "did you mean 'graphics'" in _boundaries(["A !-> graphic"], sets={"graphics": ["B"]}).unmatched(src)[0]


def test_bad_rule_syntax_is_rejected():
    with pytest.raises(SchemaError, match="rules\\[0\\]"):
        _boundaries(["nonsense"])


def test_custom_scanner(tmp_path):
    register_scanner("fake", {".fk"}, lambda text: [line.removeprefix("use ") for line in text.splitlines()])
    src = _tree(tmp_path, {"A/x.fk": "use B/y.fk\n"})
    assert len(_boundaries(["A !-> B"], scan="fake").check(src)) == 1


def test_artifacts_respect_requires_and_deny(tmp_path):
    from pyforge.boundaries import register_artifact_tool

    register_artifact_tool("fake-nm", lambda path: path.read_text().splitlines())
    (tmp_path / "lib.a").write_text("T ok::f\nT gfx::draw\n")
    artifact = {"path": "lib.a", "deny": "gfx::", "tool": "fake-nm", "requires": ["!graphics"]}
    cfg = parse_config({"project": {"name": "p"}, "options": {"graphics": {"default": True}}, "boundaries": {"artifacts": [artifact]}})
    boundaries = Boundaries(cfg.boundaries)
    assert boundaries.check_artifacts(tmp_path, {"graphics": True}) == []
    assert len(boundaries.check_artifacts(tmp_path, {"graphics": False})) == 1
