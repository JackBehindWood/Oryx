#include "doctest.h"

#include "unit/Interface/GUI/Dock/DockTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

constexpr float k_strip = 22.0f;
constexpr float k_bar = 24.0f;

struct Fixture
{
    PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    DockMetrics metrics;

    Fixture()
    {
        metrics.strip_height = k_strip;
        metrics.toolbar_height = k_bar;
        find_panel(panels, pid("a"))->toolbar = true;
        find_panel(panels, pid("b"))->toolbar = true;
        find_panel(panels, pid("vp"))->toolbar = true;
    }

    SolvedLayout solved() const { return solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) }); }
};

} // namespace

TEST_CASE("dock toolbar: tabbed panels put the strip first, a lone panel puts the toolbar first")
{
    Fixture fx;
    const SolvedLayout s = fx.solved();
    const SolvedNode& tabbed = s.nodes[1];
    CHECK(tabbed.strip.min[1] == doctest::Approx(tabbed.rect.min[1]));
    CHECK(tabbed.toolbar.min[1] == doctest::Approx(tabbed.strip.min[1] + k_strip));
    CHECK(tabbed.body.min[1] == doctest::Approx(tabbed.toolbar.min[1] + k_bar));
    CHECK(tabbed.body.size[1] == doctest::Approx(tabbed.rect.size[1] - k_strip - k_bar));

    const SolvedNode& lone = s.nodes[2];
    CHECK(lone.toolbar.min[1] == doctest::Approx(lone.rect.min[1]));
    CHECK(lone.strip.min[1] == doctest::Approx(lone.toolbar.min[1] + k_bar));
    CHECK(lone.body.min[1] == doctest::Approx(lone.strip.min[1] + k_strip));
}

TEST_CASE("dock toolbar: user style beats the panel placement, which beats the node rule")
{
    Fixture fx;
    find_panel(fx.panels, pid("b"))->toolbar_placement = ToolbarPlacement::AboveTabs;
    SolvedLayout s = fx.solved();
    CHECK(s.nodes[1].toolbar.min[1] < s.nodes[1].strip.min[1]);

    fx.metrics.style.toolbar_placement = ToolbarPlacement::BelowTabs;
    s = fx.solved();
    CHECK(s.nodes[1].toolbar.min[1] > s.nodes[1].strip.min[1]);
    CHECK(s.nodes[2].toolbar.min[1] > s.nodes[2].strip.min[1]);
}

TEST_CASE("dock toolbar: hidden toolbars and panels without one reserve nothing")
{
    Fixture fx;
    const SolvedLayout with = fx.solved();
    fx.metrics.style.toolbars = false;
    const SolvedLayout without = fx.solved();
    CHECK(is_empty(without.nodes[1].toolbar));
    CHECK(without.nodes[1].body.size[1] == doctest::Approx(without.nodes[1].rect.size[1] - k_strip));
    CHECK(with.nodes[1].body.size[1] < without.nodes[1].body.size[1]);

    Fixture none;
    find_panel(none.panels, pid("b"))->toolbar = false;
    CHECK(is_empty(none.solved().nodes[1].toolbar));
}

TEST_CASE("dock toolbar: bottom tabs keep the toolbar order relative to the strip")
{
    Fixture fx;
    fx.metrics.style.tab_position = TabPosition::Bottom;
    const SolvedLayout s = fx.solved();
    const SolvedNode& tabbed = s.nodes[1];
    CHECK(tabbed.body.min[1] == doctest::Approx(tabbed.rect.min[1]));
    CHECK(tabbed.toolbar.min[1] + tabbed.toolbar.size[1] == doctest::Approx(tabbed.rect.min[1] + tabbed.rect.size[1]));
    CHECK(tabbed.strip.min[1] + tabbed.strip.size[1] == doctest::Approx(tabbed.toolbar.min[1]));
}

TEST_CASE("dock toolbar: a float stacks title, toolbar, body")
{
    Fixture fx;
    require_applied(float_panel(fx.layout, fx.panels, pid("a"), Rect{ Vec2f(10.0f, 10.0f), Vec2f(200.0f, 150.0f) }));
    const SolvedLayout s = fx.solved();
    const SolvedFloat& parts = s.float_parts[0];
    CHECK(parts.title.size[1] == doctest::Approx(k_strip));
    CHECK(parts.toolbar.min[1] == doctest::Approx(parts.title.min[1] + k_strip));
    CHECK(parts.body.min[1] == doctest::Approx(parts.toolbar.min[1] + k_bar));
}

TEST_CASE("dock resolve_drop: reorder, dock, refusals and float")
{
    Fixture fx;
    const SolvedLayout s = fx.solved();
    const Rect a = s.nodes[1].tab_rects[0];
    const Rect b = s.nodes[1].tab_rects[1];

    DropPlan plan = resolve_drop(fx.layout, fx.panels, s, pid("b"), Vec2f(a.min[0] + 2.0f, a.min[1] + 2.0f), false);
    CHECK(plan.action == DropAction::Reorder);
    CHECK(plan.slot == 0);
    CHECK_FALSE(is_empty(plan.marker));

    plan = resolve_drop(fx.layout, fx.panels, s, pid("b"), Vec2f(b.min[0] + b.size[0] * 0.5f, b.min[1] + 2.0f), false);
    CHECK(plan.action == DropAction::None);

    const Rect vp = s.nodes[2].body;
    const Rect vp_node = s.nodes[2].rect;
    const Vec2f vp_middle(vp_node.min[0] + vp_node.size[0] * 0.5f, vp_node.min[1] + vp_node.size[1] * 0.5f);
    plan = resolve_drop(fx.layout, fx.panels, s, pid("b"), vp_middle, false);
    CHECK(plan.action == DropAction::Dock);
    CHECK(plan.node == 2);
    CHECK(plan.zone == DropZone::Centre);

    REQUIRE(add_dock_only(fx.panels, pid("b"), pid("a")));
    plan = resolve_drop(fx.layout, fx.panels, s, pid("b"), vp_middle, false);
    CHECK(plan.action == DropAction::Cancel);
    CHECK(plan.reason == DockReason::NotPermittedTarget);
    CHECK(is_empty(plan.preview));

    plan = resolve_drop(fx.layout, fx.panels, s, pid("a"), Vec2f(900.0f, 700.0f), false);
    CHECK(plan.action == DropAction::Float);
    CHECK(contains(s.surface_rect, plan.preview.min));

    plan = resolve_drop(fx.layout, fx.panels, s, pid("a"), Vec2f(vp.min[0] + 5.0f, vp.min[1] + 5.0f), true);
    CHECK(plan.action == DropAction::Float);

    set_flags(fx.panels, "a", panel_flag::all & ~panel_flag::tear_off);
    plan = resolve_drop(fx.layout, fx.panels, s, pid("a"), Vec2f(900.0f, 700.0f), false);
    CHECK(plan.action == DropAction::Cancel);
    CHECK(plan.reason == DockReason::NotPermittedFloat);
}
