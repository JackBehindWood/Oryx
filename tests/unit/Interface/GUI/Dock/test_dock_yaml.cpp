#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/TestLogCapture.h"

#include "Oryx/Interface/GUI/Dock/DockYaml.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

constexpr const char* k_golden = R"(version: 1
surfaces:
  - surface: 0
    root:
      split:
        axis: horizontal
        mode: ratio
        ratio: 0.7
        points: 0
        first:
          tabs:
            panels: [a, b]
            selected: 1
            collapsed: false
        second:
          tabs:
            panels: [vp]
            selected: 0
            collapsed: false
floats: []
homes: []
closed: []
)";

struct TempDir
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "oryx_dock_yaml_test";

    TempDir()
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~TempDir() { std::filesystem::remove_all(path); }
};

DockLayout rich_layout(const PanelTable& panels)
{
    DockLayout layout = make_sample(panels);
    require_applied(set_split(layout, panels, 0, DockSizeMode::FixedSecond, 0.37f, 123.456f));
    require_applied(dock_panel(layout, panels, pid("c"), 1, DropZone::Bottom));
    require_applied(float_panel(layout, panels, pid("d"), Rect{ Vec2f(10.1f, 20.2f), Vec2f(300.3f, 200.7f) }));
    require_applied(close_panel(layout, panels, pid("a")));
    return layout;
}

void check_round_trip(const DockLayout& layout, const PanelTable& write_panels, const PanelTable& read_panels)
{
    std::string text;
    REQUIRE(serialize_layout_to_string(layout, write_panels, text));
    DockLayout loaded;
    DockYamlStatus status = DockYamlStatus::Corrupt;
    REQUIRE(deserialize_layout_from_string(loaded, read_panels, text, &status));
    CHECK(status == DockYamlStatus::Ok);
    CHECK(equal(layout, loaded));
}

// Loads, and requires either a refusal that leaves `out` alone or a layout that passes validate.
void check_hostile(const PanelTable& panels, std::string_view text)
{
    const DockLayout sentinel = make_sample(panels);
    DockLayout out = sentinel;
    bool loaded = false;
    CHECK_NOTHROW(loaded = deserialize_layout_from_string(out, panels, text));
    if (loaded)
        CHECK_NOTHROW(validate(out));
    else
        CHECK(equal(out, sentinel));
}

// Version 0 differs from 1 only by a key readers ignore, so the step itself changes nothing.
void no_change(YAML::Node&) {}

}

TEST_CASE("DockYaml: layouts round-trip through a string")
{
    const PanelTable panels = make_panels();
    check_round_trip(DockLayout{}, panels, panels);
    check_round_trip(make_sample(panels), panels, panels);
    check_round_trip(rich_layout(panels), panels, panels);
}

TEST_CASE("DockYaml: the golden v1 document reads as the sample layout")
{
    const PanelTable panels = make_panels();
    DockLayout loaded;
    REQUIRE(deserialize_layout_from_string(loaded, panels, k_golden));
    CHECK(equal(loaded, make_sample(panels)));
}

TEST_CASE("DockYaml: the writer emits the golden v1 document")
{
    const PanelTable panels = make_panels();
    std::string text;
    REQUIRE(serialize_layout_to_string(make_sample(panels), panels, text));
    DockLayout reread;
    REQUIRE(deserialize_layout_from_string(reread, panels, text));
    CHECK(text.find("version: 1") != std::string::npos);
    CHECK(text.find("panels: [a, b]") != std::string::npos);
    CHECK(text.find("ratio: 0.7") != std::string::npos);
}

TEST_CASE("DockYaml: a layout survives only on one surface")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    layout.node_count = 1;
    layout.nodes[0].kind = DockNodeKind::Tabs;
    layout.nodes[0].count = 1;
    layout.nodes[0].tabs[0] = pid("a");
    layout.roots[1] = 0;
    check_round_trip(layout, panels, panels);
}

TEST_CASE("DockYaml: unregistered panels are kept and restore once registered")
{
    const PanelTable panels = make_panels();
    const PanelTable none;
    const DockLayout layout = rich_layout(panels);

    std::string text;
    REQUIRE(serialize_layout_to_string(layout, none, text));
    CHECK(text.find("hash:") != std::string::npos);

    DockLayout without_names;
    REQUIRE(deserialize_layout_from_string(without_names, none, text));
    CHECK(equal(layout, without_names));

    std::string again;
    REQUIRE(serialize_layout_to_string(without_names, none, again));
    CHECK(again == text);

    DockLayout with_names;
    REQUIRE(deserialize_layout_from_string(with_names, panels, text));
    CHECK(equal(layout, with_names));
}

TEST_CASE("DockYaml: migrations step a document up to the current version")
{
    const PanelTable panels = make_panels();
    const gui::detail::DockMigration table[] = { { 0, no_change } };
    constexpr const char* legacy = "version: 0\nlegacy: true\nsurfaces: []\n";

    DockLayout out;
    DockYamlStatus status = DockYamlStatus::Corrupt;
    REQUIRE(gui::detail::deserialize_with_migrations(out, panels, legacy, table, 1, &status));
    CHECK(status == DockYamlStatus::Ok);
    CHECK(out.version == k_dock_version);
    CHECK(out.node_count == 0);

    DockLayout untouched = make_sample(panels);
    CHECK_FALSE(gui::detail::deserialize_with_migrations(untouched, panels, legacy, nullptr, 0, &status));
    CHECK(status == DockYamlStatus::Corrupt);
    CHECK(equal(untouched, make_sample(panels)));
}

TEST_CASE("DockYaml: a newer version is refused and reported")
{
    const PanelTable panels = make_panels();
    DockLayout out = make_sample(panels);
    DockYamlStatus status = DockYamlStatus::Ok;
    CHECK_FALSE(deserialize_layout_from_string(out, panels, "version: 99\nsurfaces: []\n", &status));
    CHECK(status == DockYamlStatus::NewerVersion);
    CHECK(equal(out, make_sample(panels)));

    CHECK_FALSE(deserialize_layout_from_string(out, panels, "surfaces: []\n", &status));
    CHECK(status == DockYamlStatus::Corrupt);
    CHECK_FALSE(deserialize_layout_from_string(out, panels, "version: soon\nsurfaces: []\n", &status));
    CHECK(status == DockYamlStatus::Corrupt);
}

TEST_CASE("DockYaml: corrupt documents are refused with one log line each")
{
    const PanelTable panels = make_panels();
    const std::string tab = "{tabs: {panels: [a], selected: 0}}";
    const std::string split_head = "{split: {axis: horizontal, mode: ratio, points: 0, ";
    const std::vector<std::string> documents = {
        "",
        "garbage: [",
        "- 1\n- 2\n",
        "version: 1\n",
        "version: 1\nsurfaces: 3\n",
        "version: 1\nsurfaces: [{surface: 0}]\n",
        "version: 1\nsurfaces: [{surface: 5, root: " + tab + "}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: " + tab + "}, {surface: 0, root: " + tab + "}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [], selected: 0}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [a], selected: 3}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [a, a], selected: 0}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [{hash: 0}], selected: 0}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [{}], selected: 0}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {oops: 1}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [a], selected: 0}, split: {}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: " + split_head + "ratio: 0.5, first: " + tab + ", second: " + tab + "}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: {split: {axis: sideways, mode: ratio, ratio: 0.5, points: 0, first: " + tab + ", second: " + tab + "}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: " + split_head + "ratio: .nan, first: " + tab + ", second: {tabs: {panels: [b], selected: 0}}}}}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: " + tab + "}]\nfloats: [{panel: b, rect: [1, 2, 3]}]\n",
        "version: 1\nsurfaces: [{surface: 0, root: " + tab + "}]\nhomes: [{panel: b, sibling: a, zone: nowhere}]\n",
    };

    for (const std::string& text : documents)
    {
        CAPTURE(text);
        ClientLogCapture log;
        const DockLayout sentinel = make_sample(panels);
        DockLayout out = sentinel;
        DockYamlStatus status = DockYamlStatus::Ok;
        CHECK_FALSE(deserialize_layout_from_string(out, panels, text, &status));
        CHECK(status != DockYamlStatus::Ok);
        CHECK(equal(out, sentinel));
        CHECK(log.lines().size() == 1);
    }
}

TEST_CASE("DockYaml: a layout that parses but fails validate is Invalid")
{
    const PanelTable panels = make_panels();
    DockLayout out;
    DockYamlStatus status = DockYamlStatus::Ok;
    ClientLogCapture log;
    CHECK_FALSE(deserialize_layout_from_string(out, panels, "version: 1\nsurfaces: [{surface: 0, root: {tabs: {panels: [a, a], selected: 0}}}]\n", &status));
    CHECK(status == DockYamlStatus::Invalid);
}

TEST_CASE("DockYaml: out-of-range ratios and stale closed entries are normalized on load")
{
    const PanelTable panels = make_panels();
    constexpr const char* text = "version: 1\nsurfaces: [{surface: 0, root: {split: {axis: horizontal, mode: ratio, points: 0, ratio: 2, first: {tabs: {panels: [a], selected: 0}}, second: {tabs: {panels: [b], selected: 0}}}}}]\nclosed: [a]\n";
    DockLayout out;
    REQUIRE(deserialize_layout_from_string(out, panels, text));
    CHECK_NOTHROW(validate(out));
    CHECK(out.nodes[out.roots[0]].ratio < 1.0f);
    CHECK(out.closed_count == 0);
}

TEST_CASE("DockYaml: too many nodes, alias bombs, deep nesting and big files are refused")
{
    const PanelTable panels = make_panels();

    std::string bomb = "{tabs: {panels: [a], selected: 0}}";
    for (int32_t level = 1; level <= 24; ++level)
    {
        const std::string anchor = "n" + std::to_string(level);
        bomb = "{split: {axis: horizontal, mode: ratio, ratio: 0.5, points: 0, first: &" + anchor + " " + bomb + ", second: *" + anchor + "}}";
    }
    check_hostile(panels, "version: 1\nsurfaces: [{surface: 0, root: " + bomb + "}]\n");

    DockLayout out;
    CHECK_FALSE(deserialize_layout_from_string(out, panels, "version: 1\nsurfaces: [{surface: 0, root: " + bomb + "}]\n"));
    CHECK_FALSE(deserialize_layout_from_string(out, panels, std::string(20000, '[')));
    CHECK_FALSE(deserialize_layout_from_string(out, panels, std::string(k_max_dock_yaml_bytes + 1, '#')));
}

TEST_CASE("DockYaml: truncated documents never throw or half-load")
{
    const PanelTable panels = make_panels();
    ClientLogCapture log;
    std::string text;
    REQUIRE(serialize_layout_to_string(rich_layout(panels), panels, text));
    for (std::size_t length = 0; length < text.size(); ++length)
        check_hostile(panels, std::string_view(text.data(), length));
}

TEST_CASE("DockYaml: single-byte mutations never throw or half-load")
{
    const PanelTable panels = make_panels();
    ClientLogCapture log;
    std::string text;
    REQUIRE(serialize_layout_to_string(rich_layout(panels), panels, text));
    const char alphabet[] = "[]{}:,-#&*!0159.\n \"a";
    Lcg random;
    for (int32_t i = 0; i < 1500; ++i)
    {
        std::string mutated = text;
        mutated[random.below(static_cast<uint32_t>(mutated.size()))] = alphabet[random.below(sizeof(alphabet) - 1)];
        check_hostile(panels, mutated);
    }
}

TEST_CASE("DockYaml: layouts save atomically, load back and quarantine on request")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = rich_layout(panels);
    TempDir dir;
    const std::filesystem::path file = dir.path / "nested" / "layout.yaml";

    DockLayout out;
    DockYamlStatus status = DockYamlStatus::Ok;
    CHECK_FALSE(load_layout_yaml(out, panels, file, &status));
    CHECK(status == DockYamlStatus::Missing);

    REQUIRE(save_layout_yaml(layout, panels, file));
    CHECK_FALSE(std::filesystem::exists(std::filesystem::path(file) += ".tmp"));
    REQUIRE(load_layout_yaml(out, panels, file, &status));
    CHECK(status == DockYamlStatus::Ok);
    CHECK(equal(out, layout));

    REQUIRE(quarantine_layout_file(file));
    CHECK_FALSE(std::filesystem::exists(file));
    CHECK(std::filesystem::exists(std::filesystem::path(file) += ".bad"));
    CHECK_FALSE(quarantine_layout_file(file));
}
