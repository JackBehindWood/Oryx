#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/TestLogCapture.h"

#include "Oryx/Dashboard/Panel/DashboardDefaultLayout.h"
#include "Oryx/Dashboard/Panel/DashboardLayoutStore.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

gui::PanelTable dashboard_panels()
{
    gui::PanelTable table;
    REQUIRE(gui::add_panel(table, k_viewport_panel, "Board", gui::PanelKind::Viewport, 100.0f, 80.0f));
    REQUIRE(gui::add_panel(table, k_probabilities_panel, "Probabilities", gui::PanelKind::View, 80.0f, 60.0f));
    REQUIRE(gui::add_panel(table, k_values_panel, "Values", gui::PanelKind::View, 80.0f, 60.0f));
    return table;
}

struct TempDir
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "oryx_dashboard_layout_test";

    TempDir()
    {
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~TempDir() { std::filesystem::remove_all(path); }
};

bool has_panel(const gui::DockLayout& layout, std::string_view name)
{
    const gui::PanelId id = gui::make_panel_id(name);
    for (uint32_t n = 0; n < layout.node_count; ++n)
        for (uint32_t t = 0; layout.nodes[n].kind == gui::DockNodeKind::Tabs && t < layout.nodes[n].count; ++t)
            if (layout.nodes[n].tabs[t] == id)
                return true;
    return false;
}

}

TEST_CASE("default dashboard layout: the board beside tabbed views, valid and stable")
{
    const gui::PanelTable panels = dashboard_panels();
    const gui::DockLayout layout = default_dashboard_layout(panels);
    CHECK_NOTHROW(gui::validate(layout, panels, gui::ValidateFlags{ true }));
    CHECK(has_panel(layout, k_viewport_panel));
    CHECK(has_panel(layout, k_probabilities_panel));
    CHECK(has_panel(layout, k_values_panel));
    REQUIRE(layout.roots[0] != gui::k_no_node);
    CHECK(layout.nodes[layout.roots[0]].kind == gui::DockNodeKind::Split);
    CHECK(layout.nodes[layout.roots[0]].mode == gui::DockSizeMode::FixedSecond);
    CHECK(gui::equal(layout, default_dashboard_layout(panels)));

    std::string text;
    REQUIRE(gui::serialize_layout_to_string(layout, panels, text));
    gui::DockLayout reread;
    REQUIRE(gui::deserialize_layout_from_string(reread, panels, text));
    CHECK(gui::equal(layout, reread));
}

TEST_CASE("default dashboard layout: panels missing from the table are left out")
{
    gui::PanelTable only_board;
    REQUIRE(gui::add_panel(only_board, k_viewport_panel, "Board", gui::PanelKind::Viewport));
    const gui::DockLayout board = default_dashboard_layout(only_board);
    CHECK_NOTHROW(gui::validate(board, only_board, gui::ValidateFlags{ true }));
    CHECK(board.node_count == 1);

    gui::PanelTable only_views;
    REQUIRE(gui::add_panel(only_views, k_values_panel, "Values", gui::PanelKind::View));
    const gui::DockLayout views = default_dashboard_layout(only_views);
    CHECK_NOTHROW(gui::validate(views));
    CHECK(has_panel(views, k_values_panel));

    const gui::DockLayout empty = default_dashboard_layout(gui::PanelTable{});
    CHECK(empty.node_count == 0);
}

TEST_CASE("dashboard layout store: a missing file gives the default silently")
{
    const gui::PanelTable panels = dashboard_panels();
    TempDir dir;
    DashboardSettings settings;
    settings.layout_file = dir.path / "layout.yaml";
    ClientLogCapture log;
    const DashboardLayoutLoad load = load_dashboard_layout(settings, panels);
    CHECK(load.autosave_allowed);
    CHECK(gui::equal(load.layout, default_dashboard_layout(panels)));
    CHECK(log.lines().empty());
}

TEST_CASE("dashboard layout store: save then load returns the saved layout")
{
    const gui::PanelTable panels = dashboard_panels();
    TempDir dir;
    DashboardSettings settings;
    settings.layout_file = dir.path / "layout.yaml";
    gui::DockLayout layout = default_dashboard_layout(panels);
    REQUIRE(gui::set_split(layout, panels, layout.roots[0], gui::DockSizeMode::Ratio, 0.4f, 0.0f).applied);
    REQUIRE(save_dashboard_layout(settings, layout, panels));
    const DashboardLayoutLoad load = load_dashboard_layout(settings, panels);
    CHECK(load.autosave_allowed);
    CHECK(gui::equal(load.layout, layout));
}

TEST_CASE("dashboard layout store: a corrupt file is moved aside and replaced by the default")
{
    const gui::PanelTable panels = dashboard_panels();
    TempDir dir;
    DashboardSettings settings;
    settings.layout_file = dir.path / "layout.yaml";
    { std::ofstream(settings.layout_file) << "version: 1\nsurfaces: nope\n"; }
    ClientLogCapture log;
    const DashboardLayoutLoad load = load_dashboard_layout(settings, panels);
    CHECK(load.autosave_allowed);
    CHECK(gui::equal(load.layout, default_dashboard_layout(panels)));
    CHECK_FALSE(std::filesystem::exists(settings.layout_file));
    CHECK(std::filesystem::exists(std::filesystem::path(settings.layout_file) += ".bad"));
    CHECK(log.lines().size() == 1);
}

TEST_CASE("dashboard layout store: a newer file is kept and autosave is switched off")
{
    const gui::PanelTable panels = dashboard_panels();
    TempDir dir;
    DashboardSettings settings;
    settings.layout_file = dir.path / "layout.yaml";
    { std::ofstream(settings.layout_file) << "version: 99\nsurfaces: []\n"; }
    ClientLogCapture log;
    const DashboardLayoutLoad load = load_dashboard_layout(settings, panels);
    CHECK_FALSE(load.autosave_allowed);
    CHECK(gui::equal(load.layout, default_dashboard_layout(panels)));
    CHECK(std::filesystem::exists(settings.layout_file));
}
