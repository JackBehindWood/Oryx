from datetime import date

import pytest

from pyforge.boundaries import Boundaries, BoundarySpec, BoundarySuite, compile_selector, register_artifact_tool, register_scanner
from pyforge.config import SchemaError


def _tree(tmp_path, files):
    for relative, content in files.items():
        path = tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    return tmp_path


def _spec(**table) -> BoundarySpec:
    return BoundarySpec(**table)


def _messages(spec, root, **kwargs):
    return [v.message for v in Boundaries(spec, **kwargs).check(root)]


def test_forbid_unless_and_only(tmp_path):
    spec = _spec()
    a, b, ok = spec.module("A"), spec.module("B"), spec.files("a-ok", "A/ok*")
    secret, core_f = spec.files("secret", "Secret.h"), spec.files("core-f", "Core/f.cpp")
    spec.forbid(a, b, unless=ok)
    spec.only(secret, from_=core_f)
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n', "A/ok.h": '#include "B/y.h"\n', "A/s.h": '#include "Secret.h"\n', "Core/f.cpp": '#include "Secret.h"\n'})
    assert sorted(v.subject for v in Boundaries(spec).check(src)) == ["A/s.h", "A/x.h"]


def test_generated_sentences_name_the_real_modules(tmp_path):
    spec = _spec()
    game, renderer = spec.module("Game"), spec.module("Renderer")
    headless, graphics = spec.group("headless", game), spec.group("graphics", renderer)
    spec.forbid(headless, graphics, because="the core stays headless")
    src = _tree(tmp_path, {"Game/G.h": '#include "Renderer/R.h"\n'})
    assert _messages(spec, src) == ["Game must not include Renderer (the core stays headless)"]


def test_sentence_shows_the_included_path_when_it_is_not_a_module(tmp_path):
    spec = _spec()
    assets = spec.module("Assets")
    spec.forbid(assets, spec.files("window-headers", "Core/Window.h"), because="CPU assets stay headless", unless=spec.files("bridges", "Assets/Gpu*"))
    src = _tree(tmp_path, {"Assets/Img.h": '#include "Core/Window.h"\n', "Assets/GpuCache.h": '#include "Core/Window.h"\n'})
    assert _messages(spec, src) == ["Assets must not include Core/Window.h (except bridges; CPU assets stay headless)"]


def test_only_and_layers_sentences(tmp_path):
    spec = _spec()
    low, high, secret, ok = spec.module("Low"), spec.module("High"), spec.files("secrets", "Secret.h"), spec.files("allowed", "Core/f.cpp")
    spec.layers(low, high)
    spec.only(secret, from_=ok)
    src = _tree(tmp_path, {"Low/a.h": '#include "High/b.h"\n', "Other/o.cpp": '#include "Secret.h"\n'})
    assert _messages(spec, src) == ["Low must not include High, a higher layer", "Other/o.cpp"[:0] + "Secret.h may only be included by allowed"]


def test_message_replaces_and_default_extends(tmp_path):
    spec = _spec()
    a, b = spec.module("A"), spec.module("B")
    a.must_not_include(b, because="reason", message="custom {source}->{target} @{file}:{line} via {raw}")
    spec.forbid(spec.module("C"), spec.module("D"), because="why", message="{default}. See docs/boundaries.md")
    src = _tree(tmp_path, {"A/x.h": '\n#include "B/y.h"\n', "C/x.h": '#include "D/y.h"\n'})
    assert _messages(spec, src) == ["custom A->B @A/x.h:2 via B/y.h", "C must not include D (why). See docs/boundaries.md"]


def test_callable_message_and_spec_wide_default(tmp_path):
    spec = _spec(message="{default} [see docs]")
    a, b, c, d = spec.modules("A", "B", "C", "D")
    a.must_not_include(b)
    c.must_not_include(d, message=lambda include, rule: f"custom {include.file} {rule.text}")
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n', "C/x.h": '#include "D/y.h"\n'})
    assert _messages(spec, src) == ["A must not include B [see docs]", "custom C/x.h C !-> D"]


def test_unknown_placeholders_stay_literal_and_module_aliases_source(tmp_path):
    spec = _spec()
    a, b = spec.modules("A", "B")
    a.must_not_include(b, message="{module} breaks {nonsense} with {target}")
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n'})
    assert _messages(spec, src) == ["A breaks {nonsense} with B"]


def test_name_of_is_the_most_specific_module_and_ignores_groups():
    spec = _spec()
    interface, gui = spec.module("Interface"), spec.module("Interface/GUI")
    spec.group("everything-ui", interface, gui)
    spec.files("headers", "**/*.h")
    assert spec.name_of("Interface/GUI/Panel.h") == "Interface/GUI"
    assert spec.name_of("Interface/UI/Button.h") == "Interface"
    assert spec.name_of("Core/X.h") == ""
    spec.module("Core")
    assert spec.name_of("Core/X.h") == "Core"


def test_helpers_equal_their_long_forms(tmp_path):
    long, short = _spec(), _spec()
    for spec in (long, short):
        spec.modules("A", "B", "C", "D")
    la, lb, lc, ld = (long._find(n) for n in "ABCD")
    sa, sb, sc, sd = (short._find(n) for n in "ABCD")
    long.forbid(la, lb)
    long.forbid(lb, la)
    long.forbid([lc, ld], la)
    long.forbid([la, lb], [lc, ld])
    short.peers(sa, sb)
    short.above(sa, sc, sd)
    short.isolate([sa, sb], from_=[sc, sd])
    src = _tree(tmp_path, {f"{m}/x.h": "".join(f'#include "{t}/y.h"\n' for t in "ABCD" if t != m) for m in "ABCD"})
    assert sorted(v.subject for v in Boundaries(long).check(src)) == sorted(v.subject for v in Boundaries(short).check(src))
    assert len(short.rules) == 4


def test_only_included_by(tmp_path):
    spec = _spec()
    secret, ok = spec.files("secret", "Secret.h"), spec.module("Core")
    secret.only_included_by(ok, because="internal")
    src = _tree(tmp_path, {"A/x.h": '#include "Secret.h"\n', "Core/y.h": '#include "Secret.h"\n'})
    assert _messages(spec, src) == ["Secret.h may only be included by Core (internal)"]


def test_rules_reject_path_strings():
    spec = _spec()
    with pytest.raises(TypeError, match="not the string 'B'"):
        spec.forbid(spec.module("A"), "B")


def test_a_part_declared_twice_is_an_error():
    spec = _spec()
    spec.module("A")
    with pytest.raises(SchemaError, match="'A' is declared twice"):
        spec.module("A")


def test_dead_parts_are_reported_unless_allowed_empty(tmp_path):
    spec = _spec()
    spec.module("A")
    spec.module("Graphcs")
    spec.files("guard", "nowhere/**", allow_empty=True)
    src = _tree(tmp_path, {"A/x.h": "\n"})
    assert [v.subject for v in Boundaries(spec).dead_parts(src)] == ["Graphcs"]


def test_modules_accept_arguments_or_lists():
    spec = _spec()
    a, b = spec.modules("A", "B")
    c, d = spec.modules(["C", "D"])
    assert [m.label for m in (a, b, c, d)] == ["A", "B", "C", "D"]


def test_rules_record_where_they_were_declared():
    spec = _spec()
    a, b = spec.modules("A", "B")
    spec.forbid(a, b)
    a.must_not_include(b)
    assert all(rule.declared.startswith("test_boundary_spec.py:") for rule in spec.rules)


def test_discover_gives_checked_attribute_access(tmp_path):
    src = _tree(tmp_path, {"Oryx/Interface/GUI/g.h": "\n", "Oryx/Interface/UI/u.h": "\n", "Oryx/Assets/a.h": "\n"})
    spec = _spec(root=".", base="Oryx")
    tree = spec.discover(src)
    gui, assets = tree.Interface.GUI, tree["Assets"]
    assert (gui.label, assets.label) == ("Interface/GUI", "Assets")
    assert tree.Interface.GUI is gui
    gui.must_not_include(assets)
    with pytest.raises(AttributeError, match=r"did you mean 'Assets'"):
        tree.Assest
    with pytest.raises(AttributeError, match="available: GUI, UI"):
        tree.Interface.Nope
    explicit = _spec(root=".", base="Oryx")
    explicit.module("Assets")
    assert explicit.discover(src).Assets == explicit._find("Assets")
    assert spec.name_of("Interface/GUI/g.h") == "Interface/GUI"


def test_an_explicit_module_gains_children_once_discovered(tmp_path):
    src = _tree(tmp_path, {"Oryx/Interface/GUI/g.h": "\n"})
    spec = _spec(root=".", base="Oryx")
    interface = spec.module("Interface")
    with pytest.raises(AttributeError, match="only spec.discover"):
        interface.GUI
    assert spec.discover(src).Interface.GUI.label == "Interface/GUI"


def test_discover_depth_limit(tmp_path):
    src = _tree(tmp_path, {"A/B/C/x.h": "\n"})
    tree = _spec().discover(src, depth=2)
    assert tree.A.B.label == "A/B"
    with pytest.raises(AttributeError, match="deeper than discover"):
        tree.A.B.C


def test_waivers_silence_then_expire_then_go_stale(tmp_path):
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n'})

    def build():
        spec = _spec()
        a, b = spec.modules("A", "B")
        a.must_not_include(b)
        spec.waive(a, b, because="legacy", expires="2026-12-31")
        return spec

    inside = Boundaries(build(), today=lambda: date(2026, 6, 1))
    assert inside.check(src) == [] and "waived until 2026-12-31: legacy" in inside.waived[0].message
    assert inside.stale_waivers(src) == []
    late = Boundaries(build(), today=lambda: date(2027, 1, 1))
    assert "waiver expired 2026-12-31: legacy" in late.check(src)[0].message
    clean = _tree(tmp_path / "other", {"A/x.h": "\n", "B/y.h": "\n"})
    stale = Boundaries(build(), today=lambda: date(2026, 6, 1))
    assert "matches no violation" in stale.stale_waivers(clean)[0].message


def test_brace_globs_and_bare_file_name_globs():
    selector = compile_selector(["{Null,Metal}*.{h,hpp}"], {})
    assert [selector.matches(path) for path in ["NullRHI.h", "a/b/MetalRHI.hpp", "Core/X.h", "NullRHI.cpp"]] == [True, True, False, False]
    assert compile_selector(["Assets/**/Gpu*"], {}).matches("Assets/GpuAssetCache.h")


def test_scanner_resolver_maps_imports_to_paths(tmp_path):
    register_scanner("dotted", {".dl"}, lambda text: text.split(), lambda raw: raw.replace(".", "/") + ".dl")
    spec = _spec(scan="dotted")
    a, b = spec.modules("a", "b")
    a.must_not_include(b)
    src = _tree(tmp_path, {"a/x.dl": "b.y", "b/y.dl": "a.x\n"})
    assert [v.subject for v in Boundaries(spec).check(src)] == ["a/x.dl"]


def test_violations_carry_line_path_rule_and_spec(tmp_path):
    spec = _spec()
    a, b = spec.modules("A", "B")
    a.must_not_include(b)
    src = _tree(tmp_path, {"A/x.h": '// top\n\n#include "B/y.h"\n'})
    violation = Boundaries(spec).check(src)[0]
    assert (violation.path, violation.line, violation.location, violation.rule, violation.spec) == ("A/x.h", 3, "A/x.h:3", "A !-> B", "cpp")


def test_python_scanner_resolves_dotted_imports(tmp_path):
    spec = _spec(scan="python")
    ui, db = spec.modules("app/ui", "app/db")
    ui.must_not_include(db, because="the UI never talks to the database")
    src = _tree(tmp_path, {
        "app/ui/view.py": "import os\nfrom app.db.models import User\n",
        "app/ui/ok.py": "from app.core import thing\nfrom . import sibling\n",
        "app/db/models.py": "from app.ui import view\n",
    })
    assert [(v.subject, v.line) for v in Boundaries(spec).check(src)] == [("app/ui/view.py", 2)]


def test_a_suite_holds_one_spec_per_language_and_rejects_duplicate_names():
    suite = BoundarySuite()
    cpp = suite.new("cpp", root="Oryx/src", namespace="oryx")
    py = suite.new("python", root="build_system")
    assert [spec.name for spec in suite] == ["cpp", "python"] and len(suite) == 2
    assert cpp.namespace == "oryx" and py.scan == "python"
    with pytest.raises(SchemaError, match="two specs are named 'cpp'"):
        suite.new("cpp", root="other")
    suite.new("cpp", name="cpp-tools", root="tools")


NM = {
    "libA.a": {
        "defined": ["oryx::gui::Panel::draw()", "oryx::DrawList::push()", "oryx::Core::run()", "std::vector<int>::size()", "other::DrawList::x()"],
        "undefined": ["glfwInit", "glfwPoll(int)", "memcpy"],
    }
}


@pytest.fixture
def fake_nm():
    register_artifact_tool("fake", lambda path: NM[path.name]["defined"], lambda path: NM[path.name]["undefined"])


def _library_spec(tmp_path):
    src = _tree(tmp_path, {
        "src/Oryx/Gfx/a.h": "namespace oryx::gui { class Panel; } class DrawList {}; class Shared {};",
        "src/Oryx/Core/b.h": "namespace oryx { class Core {}; class Shared {}; }",
        "build/libA.a": "",
    })
    spec = _spec(root="src", base="Oryx", namespace="oryx")
    gfx, core = spec.modules("Gfx", "Core")
    return src, spec, gfx, core


def test_library_must_not_export_derives_names_from_the_source(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    spec.library("build/libA.a", tool="fake").must_not_export(gfx, because="headless build")
    found = Boundaries(spec).check_artifacts(src, {})
    assert sorted(v.message for v in found) == ["libA.a defines oryx::DrawList::push() (headless build)", "libA.a defines oryx::gui::Panel::draw() (headless build)"]


def test_library_conditions_are_inherited_and_overridable(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    lib = spec.library("build/libA.a", tool="fake", when_off="graphics")
    lib.must_not_export(gfx)
    lib.must_not_import("glfw*", when_on="always")
    boundaries = Boundaries(spec)
    assert len(boundaries.check_artifacts(src, {"graphics": False, "always": False})) == 2
    assert len(boundaries.check_artifacts(src, {"graphics": True, "always": True})) == 1
    assert boundaries.check_artifacts(src, {"graphics": True, "always": False}) == []


def test_library_must_export_and_symbol_globs_and_imports(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    lib = spec.library("build/libA.a", tool="fake")
    lib.must_export(core)
    lib.must_not_export_symbols("oryx::Core::*", because="reserved")
    lib.must_not_import("glfw*", message="{default}! ({count} hits, first {symbol})")
    messages = [v.message for v in Boundaries(spec).check_artifacts(src, {})]
    assert messages == ["libA.a defines oryx::Core::run() (reserved)", "libA.a imports glfwInit and 1 more! (2 hits, first glfwInit)"]


def test_library_must_export_reports_a_missing_module(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    gone = spec.module("Gfx/Missing", allow_empty=True)
    spec.library("build/libA.a", tool="fake").must_export(gone)
    assert "defines nothing from Gfx/Missing" in Boundaries(spec).check_artifacts(src, {})[0].message


def test_library_missing_artifact_and_namespace_requirements(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    spec.library("build/none.a", tool="fake").must_not_export(gfx)
    assert Boundaries(spec).check_artifacts(src, {})[0].message == "no artifact found; build first"
    bare = _spec(root="src", base="Oryx")
    bare.library("build/libA.a", tool="fake").must_not_export(bare.module("Gfx"))
    with pytest.raises(SchemaError, match="namespace"):
        Boundaries(bare).check_artifacts(src, {})


def test_library_reads_each_artifact_once(tmp_path):
    reads = []

    def defined(path):
        reads.append(path.name)
        return []

    register_artifact_tool("counting", defined, lambda path: [])
    src, spec, gfx, core = _library_spec(tmp_path)
    lib = spec.library("build/libA.a", tool="counting")
    lib.must_not_export(gfx)
    lib.must_export(core)
    lib.must_not_export_symbols("x*")
    Boundaries(spec).check_artifacts(src, {})
    assert reads == ["libA.a"]


def test_a_language_without_declares_cannot_derive_exports(tmp_path, fake_nm):
    register_scanner("plain", {".p"}, lambda text: [])
    src = _tree(tmp_path, {"m/a.p": "", "build/libA.a": ""})
    spec = _spec(root=".", scan="plain", namespace="x")
    spec.library("build/libA.a", tool="fake").must_not_export(spec.module("m"))
    with pytest.raises(SchemaError, match="declares="):
        Boundaries(spec).check_artifacts(src, {})


def test_cpp_declarations_include_every_namespace_component():
    from pyforge.boundaries.scanners import SCANNERS

    names = SCANNERS["cpp"].declares("namespace oryx::gui { struct Widget; enum class Mode {}; } class [[nodiscard]] Big final {};")
    assert {"oryx", "gui", "Widget", "Mode", "Big"} <= set(names)


def test_toml_tables_still_build_a_spec_with_because_message_and_why(tmp_path):
    from pyforge.config import parse_config

    cfg = parse_config({"project": {"name": "p"}, "boundaries": {"rules": [
        {"rule": "A !-> B", "because": "layering"},
        {"rule": "C !-> D", "message": "{default} (custom)"},
        {"rule": "E !-> F", "why": "{module} legacy {target}"},
    ]}})
    src = _tree(tmp_path, {"A/x.h": '#include "B/y.h"\n', "C/x.h": '#include "D/y.h"\n', "E/x.h": '#include "F/y.h"\n'})
    assert _messages(BoundarySpec.from_table(cfg.boundaries), src) == ["A must not include B (layering)", "C must not include D (custom)", "E legacy F"]


def _half(files_of, **table):
    spec = _spec(**table)
    return spec, spec.modules(*files_of)


def test_merge_combines_parts_rules_waivers_and_libraries(tmp_path, fake_nm):
    left, (a, b) = _half(["A", "B"], root="src", base="Oryx", namespace="oryx")
    a.must_not_include(b, because="left")
    left.library("build/libA.a", tool="fake").must_export(b)
    right, (b2, c) = _half(["B", "C"], root="src", base="Oryx")
    c.must_not_include(b2, because="right")
    right.waive(c, b2, because="legacy", expires="2099-01-01")
    merged = left.merge(right)
    assert merged is left and [rule.because for rule in merged.rules] == ["left", "right"]
    assert [part.label for part in merged.parts] == ["A", "B", "C"] and len(merged.waivers) == 1 and len(merged.libraries) == 1
    assert (len(right.rules), len(right.parts)) == (1, 2)
    src = _tree(tmp_path, {"src/Oryx/A/x.h": '#include "B/y.h"\n', "src/Oryx/C/x.h": '#include "B/y.h"\n', "src/Oryx/B/y.h": "\n"})
    assert _messages(merged, src) == ["A must not include B (left)"]


def test_joined_builds_a_new_spec_and_leaves_the_inputs_alone():
    one, (a,) = _half(["A"], root="r", base="b", namespace="ns")
    two, (c,) = _half(["C"], root="r", base="b")
    a.must_not_include(c)
    joined = BoundarySpec.joined(one, two, name="all")
    assert joined is not one and joined.name == "all" and joined.namespace == "ns"
    assert [part.label for part in joined.parts] == ["A", "C"] and len(one.parts) == 1 and len(joined.rules) == 1
    joined.merge(two, one)
    assert len(joined.rules) == 1 and len(joined.parts) == 2


def test_merge_rejects_specs_for_a_different_tree_or_language_or_part():
    base = _spec(root="r", base="b", namespace="x")
    with pytest.raises(SchemaError, match="root differs"):
        base.merge(_spec(root="other", base="b"))
    with pytest.raises(SchemaError, match="scan differs"):
        base.merge(_spec(root="r", base="b", scan="python"))
    with pytest.raises(SchemaError, match="namespace differs"):
        base.merge(_spec(root="r", base="b", namespace="y"))
    other = _spec(root="r", base="b")
    base.module("A")
    other.files("A", "A/*.h")
    with pytest.raises(SchemaError, match="declared differently"):
        base.merge(other)
    with pytest.raises(SchemaError, match="at least one"):
        BoundarySpec.joined()


def test_merged_discovery_registers_children_on_the_merged_spec(tmp_path):
    src = _tree(tmp_path, {"Oryx/Interface/GUI/g.h": "\n"})
    left, right = _spec(root=".", base="Oryx"), _spec(root=".", base="Oryx")
    right.discover(src).Interface
    merged = left.merge(right)
    assert merged.discover(src).Interface.GUI.label == "Interface/GUI" and merged._find("Interface/GUI") is not None and right._find("Interface/GUI") is None


def test_must_not_be_included_forbids_every_includer_except_unless(tmp_path):
    spec = _spec()
    hidden, ok = spec.files("hidden", "private/**"), spec.module("Core")
    hidden.must_not_be_included(because="internal only", unless=ok)
    src = _tree(tmp_path, {"A/x.h": '#include "private/p.h"\n', "Core/y.h": '#include "private/p.h"\n', "private/p.h": "\n"})
    assert _messages(spec, src) == ["A must not include private/p.h (except Core; internal only)"]


def test_unmatched_ignores_allow_empty_guards_and_never_suggests_a_set_containing_the_word(tmp_path):
    spec = _spec()
    a, b = spec.modules("A", "Graphcs")
    guard = spec.files("backends", "**/backends/**", allow_empty=True)
    spec.module("Graphics")
    a.must_not_include(guard, b)
    src = _tree(tmp_path, {"A/x.h": "\n", "Graphics/g.h": "\n"})
    assert Boundaries(spec).unmatched(src) == ["'Graphcs' matches nothing — did you mean 'Graphics'?"]


def test_optional_parts_feed_when_on_and_when_off_and_supply_the_default_reason(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    graphics = spec.optional("graphics", gfx, because="graphics can be disabled at build time")
    lib = spec.library("build/libA.a", tool="fake", when_off=graphics)
    lib.must_not_export(gfx)
    lib.must_not_import("glfw*", because="no window code", when_off=[graphics])
    assert [rule.requires for rule in lib.rules] == [("!graphics",), ("!graphics",)]
    messages = [v.message for v in Boundaries(spec).check_artifacts(src, {"graphics": False})]
    assert "libA.a defines oryx::DrawList::push() (graphics can be disabled at build time)" in messages
    assert any(message.endswith("(no window code)") for message in messages)
    assert Boundaries(spec).check_artifacts(src, {"graphics": True}) == []
    assert spec.optionals == [graphics] and str(graphics) == "graphics" and graphics.selection is gfx


def test_optional_rejects_empty_names_and_duplicates_and_path_strings():
    spec = _spec()
    part = spec.module("G")
    spec.optional("graphics", part)
    with pytest.raises(SchemaError, match="optional twice"):
        spec.optional("graphics", part)
    with pytest.raises(SchemaError, match="name of a build option"):
        spec.optional("", part)
    with pytest.raises(TypeError, match="not the string 'G'"):
        spec.optional("x", "G")


def test_merge_carries_optionals_over():
    one, two = _spec(), _spec()
    two.optional("graphics", two.module("G"), because="r")
    assert [o.option for o in one.merge(two).optionals] == ["graphics"]


def test_when_on_and_when_off_take_a_name_an_optional_or_a_mix(tmp_path, fake_nm):
    src, spec, gfx, core = _library_spec(tmp_path)
    graphics = spec.optional("graphics", gfx)
    lib = spec.library("build/libA.a", tool="fake", when_on="release")
    lib.must_not_export(gfx, when_off=graphics)
    lib.must_not_export(gfx, when_off="graphics")
    lib.must_not_export(gfx, when_on=[graphics, "python"], when_off=["sanitize", graphics])
    lib.must_not_export(gfx)
    assert [rule.requires for rule in lib.rules] == [("!graphics",), ("!graphics",), ("graphics", "python", "!sanitize", "!graphics"), ("release",)]
    assert len(Boundaries(spec).check_artifacts(src, {"graphics": False, "release": True})) == 6


def test_symbols_match_namespaces_and_prefixes_at_word_boundaries():
    spec = _spec()
    syms = spec.symbols("py", "oryx::python", "pybind11", prefixes=["Py", "_Py"])
    assert syms.find("void oryx::python::run()") == "oryx::python"
    assert syms.find("pybind11::cast<int>()") == "pybind11"
    assert syms.find("_PyBaseObject_Type") == "_Py" and syms.find("PyList_New") == "Py"
    assert not any(syms.find(name) for name in ["myoryx::python::x", "oryx::pythonic::x", "oryx::Core::run()", "memcpy"])
    with pytest.raises(SchemaError, match="at least one namespace or prefix"):
        spec.symbols("none")


def test_library_rules_take_symbols_objects_for_export_import_and_optional(tmp_path):
    register_artifact_tool("py", lambda path: ["oryx::python::run()", "oryx::Core::run()", "pybind11::x()"], lambda path: ["PyList_New", "_PyBaseObject_Type", "memcpy"])
    src, spec, gfx, core = _library_spec(tmp_path)
    python, cpython = spec.symbols("python", "oryx::python", "pybind11"), spec.symbols("cpython", prefixes=["Py", "_Py"])
    opt = spec.optional("python", python, because="optional at build time")
    lib = spec.library("build/libA.a", tool="py", when_off=opt)
    lib.must_not_export(python)
    lib.must_not_import(cpython, "mem*", because="no CPython")
    messages = sorted(v.message for v in Boundaries(spec).check_artifacts(src, {"python": False}))
    assert messages == [
        "libA.a defines oryx::python::run() (optional at build time)",
        "libA.a defines pybind11::x() (optional at build time)",
        "libA.a imports PyList_New (no CPython)",
        "libA.a imports _PyBaseObject_Type (no CPython)",
        "libA.a imports memcpy (no CPython)",
    ]
    assert Boundaries(spec).check_artifacts(src, {"python": True}) == []
