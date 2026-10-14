#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

DockLayout three_tabs(const PanelTable& panels)
{
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), 0, DropZone::Centre));
    return layout;
}

// Tabs [a b c*] left of the viewport, as the host needs one open: node 0 is the split, node 1 the tabs.
DockLayout hosted(const PanelTable& panels)
{
    DockLayout layout = three_tabs(panels);
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Right));
    return layout;
}

DockMetrics overlapping(float overlap)
{
    DockMetrics metrics;
    metrics.strip_button = 0.0f;
    metrics.tab_overlap = overlap;
    return metrics;
}

const Rect k_surface{ Vec2f(0.0f, 0.0f), Vec2f(400.0f, 300.0f) };

}

TEST_CASE("tab style: the sloped preset is the default and the flat one draws rectangles")
{
    CHECK(TabStyle::sloped().shape == TabShape::Sloped);
    CHECK(TabStyle::sloped().slope > 0.0f);
    CHECK(TabStyle::sloped().overlap > 0.0f);
    CHECK(TabStyle::flat().shape == TabShape::Flat);
    CHECK(TabStyle::flat().slope == 0.0f);
    CHECK(GuiTheme{}.tab_style.shape == TabShape::Sloped);
}

TEST_CASE("tab style: overlapping tabs still fill the strip exactly")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = three_tabs(panels);
    const SolvedLayout flat = solve(layout, panels, overlapping(0.0f), k_surface);
    const SolvedLayout sloped = solve(layout, panels, overlapping(6.0f), k_surface);
    const SolvedNode& plain = flat.nodes[0];
    const SolvedNode& lapped = sloped.nodes[0];
    CHECK(plain.tab_rects[2].min[0] + plain.tab_rects[2].size[0] == doctest::Approx(400.0f).epsilon(0.001));
    CHECK(lapped.tab_rects[2].min[0] + lapped.tab_rects[2].size[0] == doctest::Approx(400.0f).epsilon(0.001));
    CHECK(lapped.tab_rects[1].min[0] < lapped.tab_rects[0].min[0] + lapped.tab_rects[0].size[0]);
    CHECK(lapped.tab_rects[0].min[0] + lapped.tab_rects[0].size[0] - lapped.tab_rects[1].min[0] == doctest::Approx(6.0f));
    CHECK(lapped.tab_rects[0].size[0] > plain.tab_rects[0].size[0]);
}

TEST_CASE("tab style: a single tab has nothing to overlap")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    const SolvedLayout solved = solve(layout, panels, overlapping(6.0f), k_surface);
    CHECK(solved.nodes[0].tab_rects[0].size[0] == doctest::Approx(overlapping(6.0f).tab_max_width));
}

TEST_CASE("tab style: tabs that overflow still scroll and keep the overlap")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    for (const char* name : { "b", "c", "d", "e", "f" })
        require_applied(dock_panel(layout, panels, pid(name), 0, DropZone::Centre));
    const SolvedLayout solved = solve(layout, panels, overlapping(6.0f), Rect{ Vec2f(0.0f, 0.0f), Vec2f(160.0f, 100.0f) });
    const SolvedNode& node = solved.nodes[0];
    CHECK(node.visible_count < 6);
    CHECK(node.visible_count >= 1);
    const uint32_t first = node.first_visible;
    if (node.visible_count > 1)
        CHECK(node.tab_rects[first].min[0] + node.tab_rects[first].size[0] - node.tab_rects[first + 1].min[0] == doctest::Approx(6.0f));
    const SolvedNode& last = node;
    CHECK(last.tab_rects[first + node.visible_count - 1].min[0] + last.tab_rects[first + node.visible_count - 1].size[0] <= 160.0f + 0.01f);
}

TEST_CASE("tab style: the host draws one tab polygon per tab and the chosen one last")
{
    GuiFixture f;
    f.driver.input().surface_size = { 400.0f, 300.0f };
    DockModel& model = f.context.dock_model();
    model.panels = make_panels();
    model.layout = hosted(model.panels);
    REQUIRE(select_tab(model.layout, 1, 1).applied);
    settle_dock(model);
    const auto build = f.frame_of(
        [&]
        {
            PanelHostScope host;
            for (const char* name : { "a", "b", "c", "vp" })
            {
                PanelScope panel(name);
            }
        });
    f.driver.run_frames(4, build);
    const SolvedNode& node = f.context.dock_view().solved.nodes[1];
    uint32_t tabs = 0;
    float last_left = -1.0f;
    for (uint32_t index = 0; index < f.context.draw_list().channel_count(); ++index)
    {
        for (const PolygonCmd& polygon : f.context.draw_list().channel(index).polygons)
        {
            if (polygon.count < 6 || !contains(node.strip, Vec2f(polygon.points[0][0] + 1.0f, node.strip.min[1] + 1.0f)))
                continue;
            ++tabs;
            last_left = polygon.points[0][0];
        }
    }
    CHECK(tabs == 3);
    CHECK(last_left == doctest::Approx(node.tab_rects[1].min[0]).epsilon(0.02));
}

TEST_CASE("tab style: each tab answers for its own half of the overlap")
{
    GuiFixture f;
    f.driver.input().surface_size = { 400.0f, 300.0f };
    DockModel& model = f.context.dock_model();
    model.panels = make_panels();
    model.layout = hosted(model.panels);
    REQUIRE(select_tab(model.layout, 1, 0).applied);
    settle_dock(model);
    const auto build = f.frame_of(
        [&]
        {
            PanelHostScope host;
            for (const char* name : { "a", "b", "c", "vp" })
            {
                PanelScope panel(name);
            }
        });
    f.driver.run_frames(4, build);
    const SolvedNode& node = f.context.dock_view().solved.nodes[1];
    const float row = node.strip.min[1] + node.strip.size[1] * 0.5f;
    f.driver.move_to({ node.tab_rects[1].min[0] + 4.0f, row });
    f.driver.run_frames(2, build);
    f.driver.press();
    f.driver.frame(build);
    f.driver.release();
    f.driver.run_frames(2, build);
    CHECK(model.layout.nodes[1].selected == 1);
    f.driver.move_to({ node.tab_rects[0].min[0] + 20.0f, row });
    f.driver.run_frames(2, build);
    f.driver.press();
    f.driver.frame(build);
    f.driver.release();
    f.driver.run_frames(2, build);
    CHECK(model.layout.nodes[1].selected == 0);
}

TEST_CASE("tab style: a flat theme keeps the rectangles and the underline")
{
    GuiFixture f;
    GuiTheme theme = f.context.gui_theme();
    theme.tab_style = TabStyle::flat();
    f.context.set_theme(theme);
    f.driver.input().surface_size = { 400.0f, 300.0f };
    DockModel& model = f.context.dock_model();
    model.panels = make_panels();
    model.layout = hosted(model.panels);
    settle_dock(model);
    const auto build = f.frame_of(
        [&]
        {
            PanelHostScope host;
            for (const char* name : { "a", "b", "c" })
            {
                PanelScope panel(name);
            }
        });
    f.driver.run_frames(4, build);
    CHECK(f.context.draw_list().command_count(DrawKind::Polygon) == 0);
    CHECK(f.context.dock_view().solved.metrics.tab_overlap == 0.0f);
}

TEST_CASE("tab style: the rounded preset has radius and no slope or overlap")
{
    CHECK(TabStyle::rounded().shape == TabShape::Rounded);
    CHECK(TabStyle::rounded().radius > 0.0f);
    CHECK(TabStyle::rounded().slope == 0.0f);
    CHECK(TabStyle::rounded().overlap == 0.0f);
}

TEST_CASE("tab fit: tabs take the width their label asks for while they fit, else share the strip")
{
    PanelTable panels = make_panels();
    const DockLayout layout = three_tabs(panels);
    DockMetrics metrics;
    metrics.strip_button = 0.0f;
    metrics.tab_fit = true;
    find_panel(panels, pid("a"))->tab_width = 60.0f;
    find_panel(panels, pid("b"))->tab_width = 90.0f;
    find_panel(panels, pid("c"))->tab_width = 70.0f;
    const SolvedNode& fitted = solve(layout, panels, metrics, k_surface).nodes[0];
    CHECK(fitted.tab_rects[0].size[0] == doctest::Approx(60.0f));
    CHECK(fitted.tab_rects[1].size[0] == doctest::Approx(90.0f));
    CHECK(fitted.tab_rects[1].min[0] == doctest::Approx(60.0f));
    CHECK(fitted.tab_rects[2].size[0] == doctest::Approx(70.0f));

    metrics.tab_max_width = 1000.0f;
    find_panel(panels, pid("b"))->tab_width = 900.0f;
    const SolvedNode& crowded = solve(layout, panels, metrics, k_surface).nodes[0];
    CHECK(crowded.tab_rects[0].size[0] == doctest::Approx(crowded.tab_rects[1].size[0]));
}

TEST_CASE("tab fit: crowded tabs shrink in proportion to their labels while each stays above the minimum")
{
    PanelTable panels = make_panels();
    const DockLayout layout = three_tabs(panels);
    DockMetrics metrics;
    metrics.strip_button = 0.0f;
    metrics.tab_fit = true;
    metrics.tab_max_width = 1000.0f;
    find_panel(panels, pid("a"))->tab_width = 100.0f;
    find_panel(panels, pid("b"))->tab_width = 200.0f;
    find_panel(panels, pid("c"))->tab_width = 300.0f;
    const SolvedNode& node = solve(layout, panels, metrics, k_surface).nodes[0];
    CHECK(node.tab_rects[0].size[0] < 100.0f);
    CHECK(node.tab_rects[1].size[0] == doctest::Approx(2.0f * node.tab_rects[0].size[0]));
    CHECK(node.tab_rects[2].size[0] == doctest::Approx(3.0f * node.tab_rects[0].size[0]));
    CHECK(node.tab_rects[2].min[0] + node.tab_rects[2].size[0] == doctest::Approx(k_surface.size[0]));
}
