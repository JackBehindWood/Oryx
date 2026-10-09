#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/TestLogCapture.h"

#include "Oryx/Interface/GUI/Dock/DockSession.h"
#include "Oryx/Interface/GUI/GuiSettings.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr const char* k_board = "viewport/0";
constexpr const char* k_first = "view/first";
constexpr const char* k_second = "view/second";

// A board and two grouped views: the first beside the board at 360 pt, the second tabbed with it.
gui::PanelTable hinted_panels()
{
    gui::PanelTable table;
    REQUIRE(gui::add_panel(table, k_board, "Board", gui::PanelKind::Viewport, 100.0f, 80.0f));
    REQUIRE(gui::add_panel(table, k_first, "First", gui::PanelKind::View, 80.0f, 60.0f));
    REQUIRE(gui::add_panel(table, k_second, "Second", gui::PanelKind::View, 80.0f, 60.0f));
    for (const char* name : { k_first, k_second })
    {
        gui::PanelDesc& desc = *gui::find_panel(table, pid(name));
        desc.group = pid("views");
        desc.dock_near = pid(k_board);
        desc.dock_side = gui::DropZone::Right;
        desc.dock_size = 360.0f;
    }
    gui::find_panel(table, pid(k_second))->dock_tabbed_with = pid(k_first);
    gui::find_panel(table, pid(k_first))->order = 1;
    gui::find_panel(table, pid(k_second))->order = 2;
    return table;
}

struct TempDir
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "oryx_dock_session_test";

    TempDir()
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~TempDir() { std::filesystem::remove_all(path); }
};

bool has_panel(const gui::DockLayout& layout, std::string_view name)
{
    return gui::is_open(layout, pid(name));
}

}

TEST_CASE("build_default_layout: the board beside tabbed views, valid and stable")
{
    const gui::PanelTable panels = hinted_panels();
    const gui::DockLayout layout = gui::build_default_layout(panels);
    CHECK_NOTHROW(gui::validate(layout, panels, gui::ValidateFlags{ true }));
    CHECK(has_panel(layout, k_board));
    CHECK(has_panel(layout, k_first));
    CHECK(has_panel(layout, k_second));
    REQUIRE(layout.roots[0] != gui::k_no_node);
    const gui::DockNode& root = layout.nodes[layout.roots[0]];
    CHECK(root.kind == gui::DockNodeKind::Split);
    CHECK(root.axis == gui::DockAxis::Horizontal);
    CHECK(root.mode == gui::DockSizeMode::FixedSecond);
    CHECK(root.points == doctest::Approx(360.0f));
    CHECK(layout.node_count == 3);
    CHECK(gui::equal(layout, gui::build_default_layout(panels)));

    std::string text;
    REQUIRE(gui::serialize_layout_to_string(layout, panels, text));
    gui::DockLayout reread;
    REQUIRE(gui::deserialize_layout_from_string(reread, panels, text));
    CHECK(gui::equal(layout, reread));
}

TEST_CASE("build_default_layout: a left hint fixes the first side and order beats registration")
{
    gui::PanelTable panels = hinted_panels();
    gui::PanelDesc& first = *gui::find_panel(panels, pid(k_first));
    first.dock_side = gui::DropZone::Left;
    first.dock_size = 200.0f;
    gui::find_panel(panels, pid(k_board))->order = -1;
    gui::find_panel(panels, pid(k_second))->order = 0;
    gui::find_panel(panels, pid(k_first))->order = 1;
    const gui::DockLayout layout = gui::build_default_layout(panels);
    CHECK_NOTHROW(gui::validate(layout, panels, gui::ValidateFlags{ true }));
    bool fixed_first = false;
    for (uint32_t n = 0; n < layout.node_count; ++n)
        fixed_first = fixed_first || (layout.nodes[n].kind == gui::DockNodeKind::Split && layout.nodes[n].mode == gui::DockSizeMode::FixedFirst && layout.nodes[n].points == doctest::Approx(200.0f));
    CHECK(fixed_first);
    CHECK(has_panel(layout, k_first));
    CHECK(has_panel(layout, k_second));
}

TEST_CASE("build_default_layout: initial_open false and missing panels are left out")
{
    gui::PanelTable panels = hinted_panels();
    gui::find_panel(panels, pid(k_second))->initial_open = false;
    gui::DockLayout layout = gui::build_default_layout(panels);
    CHECK(has_panel(layout, k_first));
    CHECK_FALSE(has_panel(layout, k_second));
    CHECK_NOTHROW(gui::validate(layout, panels, gui::ValidateFlags{ true }));

    gui::PanelTable only_board;
    REQUIRE(gui::add_panel(only_board, k_board, "Board", gui::PanelKind::Viewport));
    layout = gui::build_default_layout(only_board);
    CHECK(layout.node_count == 1);

    gui::PanelTable only_views;
    REQUIRE(gui::add_panel(only_views, k_second, "Second", gui::PanelKind::View));
    gui::find_panel(only_views, pid(k_second))->dock_near = pid(k_board);
    layout = gui::build_default_layout(only_views);
    CHECK_NOTHROW(gui::validate(layout));
    CHECK(has_panel(layout, k_second));

    CHECK(gui::build_default_layout(gui::PanelTable{}).node_count == 0);
}

TEST_CASE("open_panel: views return beside the board at their hinted width, then tab together")
{
    const gui::PanelTable panels = hinted_panels();
    const gui::DockLayout initial = gui::build_default_layout(panels);
    gui::DockLayout layout = initial;

    REQUIRE(gui::close_panel(layout, panels, pid(k_first)).applied);
    REQUIRE(gui::close_panel(layout, panels, pid(k_second)).applied);
    CHECK(layout.node_count == 1);

    REQUIRE(gui::open_panel(layout, panels, pid(k_first)).applied);
    REQUIRE(layout.roots[0] != gui::k_no_node);
    CHECK(layout.nodes[layout.roots[0]].kind == gui::DockNodeKind::Split);
    CHECK(layout.nodes[layout.roots[0]].mode == gui::DockSizeMode::FixedSecond);
    CHECK(layout.nodes[layout.roots[0]].points == doctest::Approx(360.0f));

    REQUIRE(gui::open_panel(layout, panels, pid(k_second)).applied);
    CHECK_MESSAGE(gui::equal(layout, initial), gui::dump(layout, panels));
    CHECK_FALSE(gui::open_panel(layout, panels, pid(k_second)).applied);
}

TEST_CASE("open_panel: a hinted panel never in the layout tabs with the panel it names")
{
    gui::PanelTable panels = hinted_panels();
    REQUIRE(gui::add_panel(panels, "view/extra", "Extra", gui::PanelKind::View));
    gui::PanelDesc& extra = *gui::find_panel(panels, pid("view/extra"));
    extra.dock_tabbed_with = pid(k_first);
    extra.dock_near = pid(k_board);
    extra.order = 3;
    gui::DockLayout layout = gui::build_default_layout(panels);
    const uint32_t nodes = layout.node_count;
    REQUIRE(gui::close_panel(layout, panels, pid("view/extra")).applied);
    layout.home_count = 0;
    REQUIRE(gui::open_panel(layout, panels, pid("view/extra")).applied);
    CHECK(layout.node_count == nodes);
    CHECK(has_panel(layout, "view/extra"));
}

TEST_CASE("set_group_open: hides and shows a family through their recorded homes")
{
    const gui::PanelTable panels = hinted_panels();
    const gui::DockLayout initial = gui::build_default_layout(panels);
    gui::DockLayout layout = initial;
    const gui::PanelId group = pid("views");
    CHECK(gui::group_open(layout, panels, group));

    REQUIRE(gui::set_group_open(layout, panels, group, false).applied);
    CHECK_FALSE(gui::group_open(layout, panels, group));
    CHECK(has_panel(layout, k_board));
    CHECK(gui::set_group_open(layout, panels, group, false).reason == gui::DockReason::NoChange);

    REQUIRE(gui::set_group_open(layout, panels, group, true).applied);
    CHECK_MESSAGE(gui::equal(layout, initial), gui::dump(layout, panels));

    CHECK(gui::set_group_open(layout, panels, pid("nobody"), true).reason == gui::DockReason::NotFound);
    CHECK(gui::set_group_open(layout, panels, gui::PanelId{}, true).reason == gui::DockReason::BadArgument);
}

TEST_CASE("load_layout_file: a missing file gives the default silently")
{
    const gui::PanelTable panels = hinted_panels();
    TempDir dir;
    ClientLogCapture log;
    const gui::DockFileLoad load = gui::load_layout_file(dir.path / "layout.yaml", panels);
    CHECK(load.autosave_allowed);
    CHECK_FALSE(load.from_file);
    CHECK(gui::equal(load.layout, gui::build_default_layout(panels)));
    CHECK(log.lines().empty());
}

TEST_CASE("load_layout_file: save then load returns the saved layout")
{
    const gui::PanelTable panels = hinted_panels();
    TempDir dir;
    const std::filesystem::path file = dir.path / "layout.yaml";
    gui::DockLayout layout = gui::build_default_layout(panels);
    REQUIRE(gui::set_split(layout, panels, layout.roots[0], gui::DockSizeMode::Ratio, 0.4f, 0.0f).applied);
    REQUIRE(gui::save_layout_yaml(layout, panels, file));
    const gui::DockFileLoad load = gui::load_layout_file(file, panels);
    CHECK(load.from_file);
    CHECK(gui::equal(load.layout, layout));
}

TEST_CASE("load_layout_file: a corrupt file is moved aside and replaced by the default")
{
    const gui::PanelTable panels = hinted_panels();
    TempDir dir;
    const std::filesystem::path file = dir.path / "layout.yaml";
    { std::ofstream(file) << "version: 1\nsurfaces: nope\n"; }
    ClientLogCapture log;
    const gui::DockFileLoad load = gui::load_layout_file(file, panels);
    CHECK(load.autosave_allowed);
    CHECK(gui::equal(load.layout, gui::build_default_layout(panels)));
    CHECK_FALSE(std::filesystem::exists(file));
    CHECK(std::filesystem::exists(std::filesystem::path(file) += ".bad"));
    CHECK(log.lines().size() == 1);
}

TEST_CASE("load_layout_file: a newer file is kept and autosave is switched off")
{
    const gui::PanelTable panels = hinted_panels();
    TempDir dir;
    const std::filesystem::path file = dir.path / "layout.yaml";
    { std::ofstream(file) << "version: 99\nsurfaces: []\n"; }
    ClientLogCapture log;
    const gui::DockFileLoad load = gui::load_layout_file(file, panels);
    CHECK_FALSE(load.autosave_allowed);
    CHECK(gui::equal(load.layout, gui::build_default_layout(panels)));
    CHECK(std::filesystem::exists(file));
}

TEST_CASE("DockSession: a session override changes layout and default but writes nothing; a user edit is saved")
{
    TempDir dir;
    const std::filesystem::path file = dir.path / "layout.yaml";
    update_settings<GuiSettings>([&file](GuiSettings& settings) { settings.layout_file = file; });
    {
        GuiContext context;
        ContextScope<GuiContext> scope(context);
        const gui::PanelTable table = hinted_panels();
        for (uint32_t i = 0; i < table.count; ++i)
        {
            gui::PanelOptions options;
            options.title = table.descs[i].title;
            options.kind = table.descs[i].kind;
            options.group = i == 0 ? "" : "views";
            options.dock_near = i == 0 ? "" : k_board;
            options.dock_size = 360.0f;
            options.initial_open = i == 0;
            REQUIRE(gui::register_panel(table.descs[i].name, options));
        }
        gui::DockSession session;
        CHECK_FALSE(session.from_file());
        CHECK_FALSE(session.group_open("views"));

        session.override_group_open("views", true);
        CHECK(session.group_open("views"));
        CHECK(gui::equal(session.layout(), session.default_layout()));
        session.flush();
        CHECK_FALSE(std::filesystem::exists(file));

        session.set_group_open("views", false);
        session.flush();
        CHECK(std::filesystem::exists(file));
        session.reset_layout();
        CHECK(session.group_open("views"));
    }
    reset_settings();
}
