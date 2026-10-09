#include "unit/Interface/GUI/Dock/DockTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

DockMetrics plain_metrics()
{
    DockMetrics metrics;
    metrics.strip_button = 0.0f;
    return metrics;
}

const DockMetrics k_metrics = plain_metrics();
const Rect k_surface{ Vec2f(0.0f, 0.0f), Vec2f(400.0f, 300.0f) };

SolvedLayout solved_sample(const PanelTable& panels, const DockLayout& layout) { return solve(layout, panels, k_metrics, k_surface); }

void check_rect(const Rect& rect, float x, float y, float w, float h)
{
    CHECK(rect.min[0] == doctest::Approx(x));
    CHECK(rect.min[1] == doctest::Approx(y));
    CHECK(rect.size[0] == doctest::Approx(w));
    CHECK(rect.size[1] == doctest::Approx(h));
}

void check_sane(const SolvedLayout& solved)
{
    for (uint32_t n = 0; n < solved.node_count; ++n)
    {
        const SolvedNode& s = solved.nodes[n];
        for (const Rect* rect : { &s.rect, &s.strip, &s.body })
        {
            CHECK(std::isfinite(rect->min[0]));
            CHECK(std::isfinite(rect->min[1]));
            CHECK(std::isfinite(rect->size[0]));
            CHECK(std::isfinite(rect->size[1]));
            CHECK(rect->size[0] >= 0.0f);
            CHECK(rect->size[1] >= 0.0f);
        }
    }
}

}

TEST_CASE("solve: a ratio split with tab strips and bodies")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solved_sample(panels, layout);

    check_rect(solved.nodes[0].rect, 0.0f, 0.0f, 400.0f, 300.0f);
    check_rect(solved.nodes[1].rect, 0.0f, 0.0f, 277.2f, 300.0f);
    check_rect(solved.nodes[2].rect, 281.2f, 0.0f, 118.8f, 300.0f);
    check_rect(solved.nodes[1].strip, 0.0f, 0.0f, 277.2f, 22.0f);
    check_rect(solved.nodes[1].body, 0.0f, 22.0f, 277.2f, 278.0f);
    CHECK(solved.nodes[1].visible_count == 2);
    CHECK(solved.nodes[1].first_visible == 0);
    check_rect(solved.nodes[1].tab_rects[0], 0.0f, 0.0f, 138.6f, 22.0f);
    check_rect(solved.nodes[1].tab_rects[1], 138.6f, 0.0f, 138.6f, 22.0f);
    check_sane(solved);
}

TEST_CASE("solve: tab widths are capped at the maximum")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    const SolvedLayout solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(1000.0f, 400.0f) });
    check_rect(solved.nodes[0].tab_rects[1], 160.0f, 0.0f, 160.0f, 22.0f);
}

TEST_CASE("solve: fixed sizes pin one side")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(set_split(layout, panels, 0, DockSizeMode::FixedFirst, 0.5f, 100.0f));
    SolvedLayout solved = solved_sample(panels, layout);
    CHECK(solved.nodes[1].rect.size[0] == doctest::Approx(100.0f));
    CHECK(solved.nodes[2].rect.size[0] == doctest::Approx(296.0f));

    require_applied(set_split(layout, panels, 0, DockSizeMode::FixedSecond, 0.5f, 120.0f));
    solved = solved_sample(panels, layout);
    CHECK(solved.nodes[2].rect.size[0] == doctest::Approx(120.0f));
    CHECK(solved.nodes[2].rect.min[0] == doctest::Approx(280.0f));
}

TEST_CASE("solve: panel minimums clamp the ratio")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(200.0f, 300.0f) });
    CHECK(solved.nodes[2].rect.size[0] == doctest::Approx(100.0f));
    CHECK(solved.nodes[1].rect.size[0] == doctest::Approx(96.0f));
    check_sane(solved);
}

TEST_CASE("solve: a vertical split stacks first above second")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), 2, DropZone::Bottom));
    const SolvedLayout solved = solved_sample(panels, layout);
    CHECK(solved.nodes[3].rect.min[1] < solved.nodes[4].rect.min[1]);
    CHECK(solved.nodes[3].rect.size[0] == doctest::Approx(solved.nodes[4].rect.size[0]));
    check_sane(solved);
}

TEST_CASE("solve: a surface smaller than the minimums shrinks proportionally")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(50.0f, 300.0f) });
    CHECK(solved.nodes[1].rect.size[0] == doctest::Approx(46.0f * 40.0f / 140.0f));
    CHECK(solved.nodes[2].rect.size[0] == doctest::Approx(46.0f * 100.0f / 140.0f));
    check_sane(solved);
}

TEST_CASE("solve: zero, negative and tiny surfaces stay finite")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), 2, DropZone::Bottom));
    const Vec2f sizes[] = { Vec2f(0.0f, 0.0f), Vec2f(-10.0f, -10.0f), Vec2f(1.0f, 1.0f), Vec2f(3.0f, 300.0f), Vec2f(400.0f, 5.0f), Vec2f(4.0f, 4.0f) };
    for (const Vec2f& size : sizes)
    {
        const SolvedLayout solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), size });
        check_sane(solved);
    }
    const SolvedLayout flat = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(400.0f, 10.0f) });
    CHECK(flat.nodes[1].strip.size[1] <= 10.0f);
    CHECK(flat.nodes[1].body.size[1] == doctest::Approx(0.0f).epsilon(1.0));
}

TEST_CASE("solve: tabs that do not fit keep their minimum width and scroll to the selected tab")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    const char* names[] = { "a", "b", "c", "d", "e", "f" };
    for (const char* name : names)
        require_applied(dock_panel(layout, panels, pid(name), k_dock_root, DropZone::Centre));
    CHECK(layout.nodes[0].selected == 5);

    SolvedLayout solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(100.0f, 200.0f) });
    CHECK(solved.nodes[0].visible_count == 2);
    CHECK(solved.nodes[0].first_visible == 4);
    check_rect(solved.nodes[0].tab_rects[4], 0.0f, 0.0f, 48.0f, 22.0f);
    check_rect(solved.nodes[0].tab_rects[5], 48.0f, 0.0f, 48.0f, 22.0f);
    CHECK(is_empty(solved.nodes[0].tab_rects[0]));

    require_applied(select_tab(layout, 0, 0));
    solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(100.0f, 200.0f) });
    CHECK(solved.nodes[0].first_visible == 0);
    check_rect(solved.nodes[0].tab_rects[0], 0.0f, 0.0f, 48.0f, 22.0f);

    solved = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(20.0f, 200.0f) });
    CHECK(solved.nodes[0].visible_count == 1);
    check_rect(solved.nodes[0].tab_rects[0], 0.0f, 0.0f, 20.0f, 22.0f);
    check_sane(solved);
}

TEST_CASE("solve: a collapsed stack shrinks to its strip and has no body")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), 1, DropZone::Bottom));
    int32_t collapsed_node = k_no_node;
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && layout.nodes[n].tabs[0] == pid("c"))
            collapsed_node = static_cast<int32_t>(n);
    REQUIRE(collapsed_node != k_no_node);
    require_applied(set_collapsed(layout, panels, collapsed_node, true));

    const SolvedLayout solved = solved_sample(panels, layout);
    const SolvedNode& node = solved.nodes[collapsed_node];
    CHECK(node.rect.size[1] == doctest::Approx(22.0f));
    CHECK(is_empty(node.body));
    check_sane(solved);
}

TEST_CASE("solve: floats are raised to the panel minimum and clamped inside the surface")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    PanelTable tearable = panels;
    set_flags(tearable, "c", panel_flag::all);
    require_applied(float_panel(layout, tearable, pid("c"), Rect{ Vec2f(-50.0f, 1000.0f), Vec2f(10.0f, 10.0f) }));
    require_applied(float_panel(layout, tearable, pid("d"), Rect{ Vec2f(10.0f, 10.0f), Vec2f(5000.0f, 5000.0f) }));
    layout.floats[1].surface = 1;

    const SolvedLayout solved = solved_sample(panels, layout);
    check_rect(solved.floats[0], 0.0f, 300.0f - 52.0f, 40.0f, 52.0f);
    CHECK(is_empty(solved.floats[1]));
    CHECK(solved.float_count == 2);

    layout.floats[1].surface = 0;
    const SolvedLayout both = solved_sample(panels, layout);
    check_rect(both.floats[1], 0.0f, 0.0f, 400.0f, 300.0f);
}

TEST_CASE("solve: an empty surface and a second surface")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    SolvedLayout solved = solved_sample(panels, layout);
    CHECK(solved.node_count == 0);

    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    solved = solve(layout, panels, k_metrics, k_surface, 1);
    CHECK(is_empty(solved.nodes[0].rect));
}

TEST_CASE("strip_at: only the strip of a Tabs node answers")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solved_sample(panels, layout);
    CHECK(strip_at(layout, solved, Vec2f(-5.0f, 10.0f)) == k_no_node);
    CHECK(strip_at(layout, solved, Vec2f(100.0f, 150.0f)) == k_no_node);
    CHECK(strip_at(layout, solved, Vec2f(279.0f, 10.0f)) == k_no_node);
    CHECK(strip_at(layout, solved, Vec2f(100.0f, 10.0f)) == 1);
    CHECK(strip_at(layout, solved, Vec2f(340.0f, 10.0f)) == 2);
}

TEST_CASE("resolve_drop: with room to spare an inner preview is the rect the dock then takes")
{
    const PanelTable panels = make_panels();
    const Rect big{ Vec2f(0.0f, 0.0f), Vec2f(1000.0f, 600.0f) };
    const DropZone zones[] = { DropZone::Left, DropZone::Right, DropZone::Top, DropZone::Bottom };
    for (const DropZone zone : zones)
    {
        DockLayout layout = make_sample(panels);
        const SolvedLayout before = solve(layout, panels, k_metrics, big);
        const Rect& node = before.nodes[2].rect;
        const DropGuides guides = drop_guides(layout, panels, before, pid("a"), Vec2f(node.min[0] + node.size[0] * 0.5f, 300.0f));
        const int32_t index = guide_index(guides, 2, zone);
        REQUIRE(index >= 0);
        const Rect& box = guides.guides[index].rect;
        const DropPlan plan = resolve_drop(layout, panels, before, guides, pid("a"), Vec2f(box.min[0] + box.size[0] * 0.5f, box.min[1] + box.size[1] * 0.5f), false);
        REQUIRE(plan.action == DropAction::Dock);
        REQUIRE(plan.node == 2);
        REQUIRE(plan.zone == zone);

        require_applied(dock_panel(layout, panels, pid("a"), 2, zone));
        const SolvedLayout after = solve(layout, panels, k_metrics, big);
        int32_t placed = k_no_node;
        for (uint32_t n = 0; n < layout.node_count; ++n)
            if (layout.nodes[n].kind == DockNodeKind::Tabs && layout.nodes[n].tabs[0] == pid("a"))
                placed = static_cast<int32_t>(n);
        REQUIRE(placed != k_no_node);
        const Rect& rect = after.nodes[placed].rect;
        check_rect(plan.preview, rect.min[0], rect.min[1], rect.size[0], rect.size[1]);
    }
}

TEST_CASE("solve: a strip reserves the collapse button when every tab may collapse")
{
    PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const DockMetrics metrics;
    SolvedLayout solved = solve(layout, panels, metrics, k_surface);
    const SolvedNode& tabs = solved.nodes[1];
    CHECK(tabs.collapse_button.size[0] == doctest::Approx(metrics.strip_button));
    CHECK(tabs.collapse_button.min[0] + tabs.collapse_button.size[0] == doctest::Approx(tabs.strip.min[0] + tabs.strip.size[0]));
    CHECK(tabs.tab_rects[1].min[0] + tabs.tab_rects[1].size[0] <= tabs.collapse_button.min[0] + 0.001f);

    set_flags(panels, "b", panel_flag::all & ~panel_flag::collapse);
    solved = solve(layout, panels, metrics, k_surface);
    CHECK(is_empty(solved.nodes[1].collapse_button));
    CHECK(solved.nodes[1].tab_rects[1].min[0] + solved.nodes[1].tab_rects[1].size[0] == doctest::Approx(solved.nodes[1].strip.size[0]));
}

TEST_CASE("solve: a split reports the gap between its children as the splitter")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const Rect& first = solved.nodes[1].rect;
    const Rect& second = solved.nodes[2].rect;
    const Rect& gap = solved.nodes[0].splitter;
    CHECK(gap.min[0] == doctest::Approx(first.min[0] + first.size[0]));
    CHECK(gap.min[0] + gap.size[0] == doctest::Approx(second.min[0]));
    CHECK(gap.size[0] == doctest::Approx(k_metrics.splitter));
    CHECK(gap.size[1] == doctest::Approx(k_surface.size[1]));
}
