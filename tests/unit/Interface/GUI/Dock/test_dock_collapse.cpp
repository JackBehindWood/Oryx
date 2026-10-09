#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

DockMetrics scaled_metrics(float scale)
{
    DockMetrics metrics;
    metrics.strip_height *= scale;
    metrics.splitter *= scale;
    metrics.tab_min_width *= scale;
    metrics.tab_max_width *= scale;
    metrics.strip_button *= scale;
    metrics.root_edge *= scale;
    return metrics;
}

Vec2f centre(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }

bool inside(const Rect& inner, const Rect& outer)
{
    return inner.min[0] >= outer.min[0] - 0.001f && inner.min[1] >= outer.min[1] - 0.001f && inner.min[0] + inner.size[0] <= outer.min[0] + outer.size[0] + 0.001f && inner.min[1] + inner.size[1] <= outer.min[1] + outer.size[1] + 0.001f;
}

int32_t node_holding(const DockLayout& layout, PanelId panel)
{
    for (uint32_t n = 0; n < layout.node_count; ++n)
        for (uint32_t t = 0; layout.nodes[n].kind == DockNodeKind::Tabs && t < layout.nodes[n].count; ++t)
            if (layout.nodes[n].tabs[t] == panel)
                return static_cast<int32_t>(n);
    return k_no_node;
}

// Nodes of make_sample: 0 split h (0.7), 1 tabs [a b*], 2 tabs [vp*].
struct CollapseScene
{
    GuiFixture f;
    DockLayout& layout = f.context.dock_model().layout;

    CollapseScene()
    {
        f.driver.input().surface_size = { 800.0f, 600.0f };
        model().panels = make_panels();
        layout = make_sample(model().panels);
        settle_dock(model());
        frames(4);
    }

    DockView& panel_host() { return f.context.dock_view(); }
    DockModel& model() { return f.context.dock_model(); }
    const SolvedLayout& solved() { return panel_host().solved; }

    void draw()
    {
        PanelHostScope host;
        for (const char* name : { "a", "b", "vp" })
        {
            PanelScope panel(name);
            if (panel.visible())
                gui::label("body");
        }
    }

    void frames(uint32_t count = 3)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.run_frames(count, build);
    }

    void click(const Vec2f& at)
    {
        auto build = f.frame_of([this] { draw(); });
        f.driver.click(at, build);
        frames(2);
    }

    void press_at(const Vec2f& at)
    {
        f.driver.move_to(at);
        frames(1);
        f.driver.press();
        frames(1);
    }

    void move_to(const Vec2f& at)
    {
        const Vec2f from = f.driver.input().pointer.position;
        for (uint32_t step = 1; step <= 6; ++step)
        {
            f.driver.move_to(from + (at - from) * (static_cast<float>(step) / 6.0f));
            frames(1);
        }
    }

    void release()
    {
        f.driver.release();
        frames(3);
    }

    Rect close_of(const Rect& tab)
    {
        const float pad = f.context.gui_theme().tab.padding.right * 0.5f;
        return Rect{ Vec2f(tab.min[0] + tab.size[0] - pad - 14.0f, tab.min[1] + (tab.size[1] - 14.0f) * 0.5f), Vec2f(14.0f, 14.0f) };
    }
};

} // namespace

TEST_CASE("solve: a collapsed column of a horizontal split keeps its width, title, close room and expander at every scale")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(set_collapsed(layout, panels, 1, true));
    for (const float scale : { 1.0f, 1.5f, 2.0f })
    {
        const DockMetrics metrics = scaled_metrics(scale);
        const SolvedLayout solved = solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) });
        const SolvedNode& rail = solved.nodes[1];
        CHECK(rail.collapsed);
        CHECK(rail.rail);
        CHECK(rail.rect.size[0] >= rail_width(metrics));
        CHECK(rail.rect.size[0] == doctest::Approx((800.0f - metrics.splitter) * 0.7f));
        CHECK(rail.rect.size[1] == doctest::Approx(600.0f));
        CHECK(is_empty(rail.body));
        REQUIRE_FALSE(is_empty(rail.collapse_button));
        CHECK(inside(rail.collapse_button, rail.rect));
        REQUIRE_FALSE(is_empty(rail.tab_rects[rail.first_visible]));
        CHECK(rail.tab_rects[rail.first_visible].min[0] + rail.tab_rects[rail.first_visible].size[0] <= rail.collapse_button.min[0] + 0.001f);
    }
}

TEST_CASE("solve: a collapsed stack of a vertical split keeps the strip height and is not a rail")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Bottom));
    const int32_t top = node_holding(layout, pid("a"));
    require_applied(set_collapsed(layout, panels, top, true));
    const DockMetrics metrics;
    const SolvedLayout solved = solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) });
    CHECK_FALSE(solved.nodes[top].rail);
    CHECK(solved.nodes[top].rect.size[1] == doctest::Approx(metrics.strip_height));
    REQUIRE_FALSE(is_empty(solved.nodes[top].collapse_button));
}

TEST_CASE("solve: every collapsed node keeps an expander across surface sizes and scales")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(set_collapsed(layout, panels, 1, true));
    for (const float scale : { 1.0f, 1.5f, 2.0f })
        for (float w = 40.0f; w <= 900.0f; w += 37.0f)
            for (float h = 30.0f; h <= 700.0f; h += 41.0f)
            {
                const SolvedLayout solved = solve(layout, panels, scaled_metrics(scale), Rect{ Vec2f(0.0f, 0.0f), Vec2f(w, h) });
                const SolvedNode& node = solved.nodes[1];
                if (!is_empty(node.rect))
                    CHECK_FALSE(is_empty(node.collapse_button));
            }
}

TEST_CASE("solve: collapsed siblings inside a nested split shrink to their minimum instead of keeping a ratio")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), k_dock_root, DropZone::Bottom));
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Right));
    const int32_t a = node_holding(layout, pid("a"));
    const int32_t c = node_holding(layout, pid("c"));
    require_applied(set_collapsed(layout, panels, a, true));
    require_applied(set_collapsed(layout, panels, c, true));
    REQUIRE(all_collapsed(layout, layout.nodes[layout.roots[0]].first));

    const DockMetrics metrics;
    const SolvedLayout solved = solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) });
    CHECK(solved.nodes[a].rect.size[0] >= rail_width(metrics));
    CHECK(solved.nodes[c].rect.size[0] == doctest::Approx(solved.nodes[a].rect.size[0]));
    CHECK(solved.nodes[a].strip.size[1] == doctest::Approx(metrics.strip_height));
    CHECK(solved.nodes[c].strip.size[1] == doctest::Approx(metrics.strip_height));
    CHECK_FALSE(is_empty(solved.nodes[a].collapse_button));
    CHECK_FALSE(is_empty(solved.nodes[c].collapse_button));
}

TEST_CASE("solve: an all-collapsed root is laid out expanded and set_collapsed refuses to create the state")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    CHECK(set_collapsed(layout, panels, 0, true).reason == DockReason::Collapsed);
    CHECK(layout.nodes[0].collapsed == 0);

    layout.nodes[0].collapsed = 1;
    const SolvedLayout solved = solve(layout, panels, DockMetrics{}, Rect{ Vec2f(0.0f, 0.0f), Vec2f(400.0f, 300.0f) });
    CHECK_FALSE(solved.nodes[0].collapsed);
    CHECK_FALSE(is_empty(solved.nodes[0].body));
}

TEST_CASE("set_collapsed: collapsing the last expanded node of a tree is refused, expanding never is")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), k_dock_root, DropZone::Right));
    const int32_t a = node_holding(layout, pid("a"));
    const int32_t b = node_holding(layout, pid("b"));
    require_applied(set_collapsed(layout, panels, a, true));
    CHECK(set_collapsed(layout, panels, b, true).reason == DockReason::Collapsed);
    require_applied(set_collapsed(layout, panels, a, false));
    require_applied(set_collapsed(layout, panels, b, true));
}

TEST_CASE("splits: a collapsed column stays resizable, a collapsed row (one strip tall) does not")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(set_collapsed(layout, panels, 1, true));
    CHECK(can_resize_split(layout, panels, 0));
    require_applied(set_split(layout, panels, 0, DockSizeMode::Ratio, 0.4f, 0.0f));

    DockLayout rows;
    require_applied(dock_panel(rows, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(rows, panels, pid("vp"), k_dock_root, DropZone::Bottom));
    require_applied(set_collapsed(rows, panels, node_holding(rows, pid("a")), true));
    CHECK_FALSE(can_resize_split(rows, panels, rows.roots[0]));
    CHECK(set_split(rows, panels, rows.roots[0], DockSizeMode::Ratio, 0.4f, 0.0f).reason == DockReason::Collapsed);
    require_applied(set_collapsed(rows, panels, node_holding(rows, pid("a")), false));
    CHECK(can_resize_split(rows, panels, rows.roots[0]));
}

TEST_CASE("collapse permissions: every flag combination agrees across insert, set_collapsed, normalize and validate")
{
    for (uint32_t bits = 0; bits < 64; ++bits)
    {
        PanelTable panels = make_panels();
        DockLayout layout = make_sample(panels);
        DockLayout fresh = layout;
        set_flags(panels, "a", static_cast<uint8_t>(bits));
        const bool may_collapse = (bits & panel_flag::collapse) != 0;
        // Dock the flagged panel's neighbour into node 1 and force the collapsed state to model a hand-edited file.
        layout.nodes[1].collapsed = 1;

        if (may_collapse)
        {
            CHECK_NOTHROW(validate(layout, panels, ValidateFlags{}));
        }
        else
        {
            CHECK_THROWS_AS(validate(layout, panels, ValidateFlags{}), oryx::Error);
            DockLayout healed = layout;
            normalize(healed, panels);
            CHECK(healed.nodes[1].collapsed == 0);
            CHECK_NOTHROW(validate(healed, panels, ValidateFlags{}));
        }

        const DockResult result = set_collapsed(fresh, panels, 1, true);
        CHECK(result.applied == may_collapse);
        if (!may_collapse)
            CHECK(result.reason == DockReason::NotPermittedCollapse);
    }
}

TEST_CASE("dock_panel: dropping a panel into a collapsed stack expands it, a viewport included")
{
    PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), k_dock_root, DropZone::Bottom));
    const int32_t stack = node_holding(layout, pid("a"));
    require_applied(set_collapsed(layout, panels, stack, true));
    require_applied(dock_panel(layout, panels, pid("c"), stack, DropZone::Centre));
    CHECK(layout.nodes[node_holding(layout, pid("c"))].collapsed == 0);

    require_applied(set_collapsed(layout, panels, node_holding(layout, pid("a")), true));
    require_applied(dock_panel(layout, panels, pid("vp"), node_holding(layout, pid("a")), DropZone::Centre));
    const int32_t merged = node_holding(layout, pid("vp"));
    CHECK(layout.nodes[merged].collapsed == 0);
    CHECK_NOTHROW(validate(layout, panels, ValidateFlags{ true }));
}

TEST_CASE("open_panel: a panel that was never placed avoids the viewport's stack")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Right));
    REQUIRE(node_holding(layout, pid("vp")) < node_holding(layout, pid("a")));
    require_applied(open_panel(layout, panels, pid("c")));
    CHECK(node_holding(layout, pid("c")) == node_holding(layout, pid("a")));

    DockLayout alone;
    require_applied(dock_panel(alone, panels, pid("vp"), k_dock_root, DropZone::Centre));
    require_applied(open_panel(alone, panels, pid("c")));
    CHECK(node_holding(alone, pid("c")) != node_holding(alone, pid("vp")));
    CHECK_NOTHROW(validate(alone, panels, ValidateFlags{ true }));
}

TEST_CASE("drop feedback: a full tab stack and a full float pool are refused with their reason")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    for (const char* name : { "a", "b", "c", "d", "e", "f", "g", "h" })
        require_applied(dock_panel(layout, panels, pid(name), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("i"), k_dock_root, DropZone::Right));
    const int32_t full = node_holding(layout, pid("a"));
    REQUIRE(layout.nodes[full].count == k_max_dock_tabs);

    const DockMetrics metrics;
    const SolvedLayout solved = solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(1000.0f, 800.0f) });
    const Vec2f over_body = centre(solved.nodes[full].body);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("i"), over_body);
    bool found = false;
    for (uint32_t g = 0; g < guides.count; ++g)
        if (guides.guides[g].node == full && guides.guides[g].zone == DropZone::Centre)
        {
            found = true;
            CHECK_FALSE(guides.guides[g].allowed);
            CHECK(guides.guides[g].reason == DockReason::TabsFull);
        }
    CHECK(found);
    const DropPlan plan = resolve_drop(layout, panels, solved, pid("i"), over_body, false);
    CHECK(plan.action == DropAction::Cancel);
    CHECK(plan.reason == DockReason::TabsFull);

    DockLayout floats;
    for (const char* name : { "a", "b", "c", "d", "e", "f", "g", "h" })
        require_applied(float_panel(floats, panels, pid(name), Rect{ Vec2f(10.0f, 10.0f), Vec2f(200.0f, 150.0f) }));
    require_applied(dock_panel(floats, panels, pid("i"), k_dock_root, DropZone::Centre));
    const SolvedLayout solved_floats = solve(floats, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(1000.0f, 800.0f) });
    const DropPlan outside = resolve_drop(floats, panels, solved_floats, pid("i"), Vec2f(1500.0f, 400.0f), false);
    CHECK(outside.action == DropAction::Cancel);
    CHECK(outside.reason == DockReason::FloatsFull);
}

TEST_CASE("float_min_size and clamp_float: the floor includes the title bar and the surface wins when smaller")
{
    PanelTable panels = make_panels();
    const DockMetrics metrics;
    const Vec2f floor = float_min_size(panels, pid("a"), metrics);
    CHECK(floor[0] == doctest::Approx(40.0f));
    CHECK(floor[1] == doctest::Approx(30.0f + metrics.strip_height));
    const Rect surface{ Vec2f(0.0f, 0.0f), Vec2f(500.0f, 400.0f) };
    const Rect fitted = clamp_float(Rect{ Vec2f(490.0f, 395.0f), Vec2f(-20.0f, 1.0f) }, floor, surface);
    CHECK(fitted.size[0] == doctest::Approx(40.0f));
    CHECK(fitted.size[1] == doctest::Approx(floor[1]));
    CHECK(inside(fitted, surface));
    const Rect tiny = clamp_float(Rect{ Vec2f(0.0f, 0.0f), Vec2f(100.0f, 100.0f) }, floor, Rect{ Vec2f(0.0f, 0.0f), Vec2f(20.0f, 20.0f) });
    CHECK(tiny.size[0] == doctest::Approx(20.0f));
}

TEST_CASE("panel host: a collapsed column keeps its title, close button and expander, and the chevron expands it")
{
    CollapseScene scene;
    scene.click(centre(scene.solved().nodes[1].collapse_button));
    REQUIRE(scene.layout.nodes[1].collapsed == 1);
    const SolvedNode& rail = scene.solved().nodes[1];
    REQUIRE(rail.rail);
    CHECK(rail.rect.size[0] >= rail_width(scene.solved().metrics));

    scene.click(centre(rail.collapse_button));
    CHECK(scene.layout.nodes[1].collapsed == 0);
    CHECK_FALSE(is_empty(scene.solved().nodes[1].body));
}

TEST_CASE("panel host: switching tabs inside a collapsed stack keeps it collapsed")
{
    CollapseScene scene;
    require_applied(set_collapsed(scene.layout, scene.model().panels, 1, true));
    scene.frames(3);
    const uint32_t before = scene.layout.nodes[1].selected;
    const uint32_t other = before == 0 ? 1u : 0u;
    for (uint32_t round = 0; round < 2; ++round)
    {
        const SolvedNode& rail = scene.solved().nodes[1];
        REQUIRE(rail.rail);
        const uint32_t target = round == 0 ? other : before;
        scene.click(centre(rail.tab_rects[target]));
        CHECK(scene.layout.nodes[1].selected == target);
        CHECK(scene.layout.nodes[1].collapsed == 1);
        CHECK(scene.model().focused == scene.layout.nodes[1].tabs[target]);
    }
}

TEST_CASE("panel host: a rail closes its panel through the close button next to the title")
{
    CollapseScene scene;
    require_applied(set_collapsed(scene.layout, scene.model().panels, 1, true));
    scene.frames(3);
    const SolvedNode& rail = scene.solved().nodes[1];
    const PanelId shown = scene.layout.nodes[1].tabs[rail.first_visible];
    scene.click(centre(scene.close_of(rail.tab_rects[rail.first_visible])));
    CHECK_FALSE(is_open(scene.layout, shown));
    CHECK_NOTHROW(validate(scene.layout, scene.model().panels, ValidateFlags{ true }));
}

TEST_CASE("panel host: a collapsed column can be widened with its splitter")
{
    CollapseScene scene;
    require_applied(set_collapsed(scene.layout, scene.model().panels, 1, true));
    scene.frames(3);
    const float before = scene.layout.nodes[0].ratio;
    const Vec2f gap = centre(scene.solved().nodes[0].splitter);
    scene.press_at(gap);
    scene.move_to(gap + Vec2f(60.0f, 0.0f));
    scene.release();
    CHECK(scene.layout.nodes[0].ratio > before);
    CHECK(scene.layout.nodes[1].collapsed == 1);
}

TEST_CASE("panel host: dragging a splitter into its clamp and back lands on the starting ratio")
{
    CollapseScene scene;
    const float start = scene.layout.nodes[0].ratio;
    const Vec2f gap = centre(scene.solved().nodes[0].splitter);
    scene.press_at(gap);
    scene.move_to(Vec2f(-300.0f, gap[1]));
    CHECK(scene.layout.nodes[0].ratio < start);
    scene.move_to(gap);
    scene.release();
    CHECK(scene.layout.nodes[0].ratio == doctest::Approx(start).epsilon(0.01));
}

TEST_CASE("panel host: a float grip dragged far below its minimum stays valid, saved and reloaded")
{
    CollapseScene scene;
    PanelTable& panels = scene.model().panels;
    require_applied(float_panel(scene.layout, panels, pid("b"), Rect{ Vec2f(100.0f, 100.0f), Vec2f(300.0f, 200.0f) }));
    scene.frames(4);
    const Rect start = scene.solved().floats[0];
    const Vec2f corner(start.min[0] + start.size[0] - 3.0f, start.min[1] + start.size[1] - 3.0f);
    scene.press_at(corner);
    scene.move_to(corner - Vec2f(900.0f, 900.0f));

    const Vec2f floor = float_min_size(panels, pid("b"), scene.solved().metrics);
    REQUIRE(scene.layout.float_count == 1);
    CHECK(scene.layout.floats[0].rect.size[0] >= floor[0] - 0.001f);
    CHECK(scene.layout.floats[0].rect.size[1] >= floor[1] - 0.001f);
    CHECK_NOTHROW(validate(scene.layout, panels, ValidateFlags{ true }));

    scene.move_to(corner);
    scene.release();
    CHECK(scene.layout.floats[0].rect.size[0] == doctest::Approx(start.size[0]).epsilon(0.02));
    CHECK(scene.layout.floats[0].rect.size[1] == doctest::Approx(start.size[1]).epsilon(0.02));

    std::string yaml;
    REQUIRE(serialize_layout_to_string(scene.layout, panels, yaml));
    DockLayout loaded;
    REQUIRE(deserialize_layout_from_string(loaded, panels, yaml));
    CHECK(equal(loaded, scene.layout));
}

TEST_CASE("panel host: focus never points at a panel that is closed or undone away")
{
    CollapseScene scene;
    PanelTable& panels = scene.model().panels;
    focus_panel("a");
    require_applied(close_panel(scene.layout, panels, pid("a")));
    scene.frames(2);
    CHECK_FALSE(is_valid(scene.model().focused));

    focus_panel("b");
    scene.frames(2);
    CHECK(is_panel_focused("b"));
    require_applied(float_panel(scene.layout, panels, pid("b"), Rect{ Vec2f(10.0f, 10.0f), Vec2f(200.0f, 150.0f) }));
    scene.frames(2);
    CHECK(is_panel_focused("b"));
}

TEST_CASE("yaml: a saved layout with a viewport in a collapsed node loads healed instead of being rejected")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    layout.nodes[2].collapsed = 1;
    std::string yaml;
    REQUIRE(serialize_layout_to_string(layout, panels, yaml));
    DockLayout loaded;
    REQUIRE(deserialize_layout_from_string(loaded, panels, yaml));
    CHECK(loaded.nodes[2].collapsed == 0);
    CHECK_NOTHROW(validate(loaded, panels, ValidateFlags{ true }));
}

TEST_CASE("yaml: a sibling-less home survives save and load")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(close_panel(layout, panels, pid("a")));
    require_applied(dock_panel(layout, panels, pid("b"), k_dock_root, DropZone::Right));
    require_applied(close_panel(layout, panels, pid("b")));
    std::string yaml;
    REQUIRE(serialize_layout_to_string(layout, panels, yaml));
    DockLayout loaded;
    REQUIRE(deserialize_layout_from_string(loaded, panels, yaml));
    CHECK(equal(loaded, layout));
}
