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
const Rect k_surface{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) };

Vec2f centre(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }

bool rects_overlap(const Rect& a, const Rect& b)
{
    return a.min[0] < b.min[0] + b.size[0] && b.min[0] < a.min[0] + a.size[0] && a.min[1] < b.min[1] + b.size[1] && b.min[1] < a.min[1] + a.size[1];
}

bool within(const Rect& outer, const Rect& inner)
{
    return inner.min[0] >= outer.min[0] - 0.001f && inner.min[1] >= outer.min[1] - 0.001f && inner.min[0] + inner.size[0] <= outer.min[0] + outer.size[0] + 0.001f && inner.min[1] + inner.size[1] <= outer.min[1] + outer.size[1] + 0.001f;
}

void check_well_formed(const DropGuides& guides, const SolvedLayout& solved)
{
    for (uint32_t i = 0; i < guides.count; ++i)
    {
        CHECK(within(solved.surface_rect, guides.guides[i].rect));
        if (guides.guides[i].node != k_dock_root)
            CHECK(within(solved.nodes[guides.guides[i].node].rect, guides.guides[i].rect));
        for (uint32_t j = i + 1; j < guides.count; ++j)
            CHECK_FALSE(rects_overlap(guides.guides[i].rect, guides.guides[j].rect));
    }
}

int32_t node_of(const DockLayout& layout, PanelId panel)
{
    for (uint32_t n = 0; n < layout.node_count; ++n)
        for (uint32_t t = 0; layout.nodes[n].kind == DockNodeKind::Tabs && t < layout.nodes[n].count; ++t)
            if (layout.nodes[n].tabs[t] == panel)
                return static_cast<int32_t>(n);
    return k_no_node;
}

uint32_t count_zone(const DropGuides& guides, DropZone zone, bool outer)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < guides.count; ++i)
        count += guides.guides[i].zone == zone && (guides.guides[i].node == k_dock_root) == outer ? 1u : 0u;
    return count;
}

}

TEST_CASE("drop_guides: a compass on the hovered node and four outer guides, all inside and apart")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), centre(solved.nodes[2].body));
    REQUIRE(guides.count == 9);
    check_well_formed(guides, solved);
    for (DropZone zone : { DropZone::Left, DropZone::Right, DropZone::Top, DropZone::Bottom })
        CHECK(count_zone(guides, zone, true) == 1);
    for (uint32_t i = 0; i < 4; ++i)
        CHECK(guides.guides[i].outer);
    for (uint32_t i = 4; i < guides.count; ++i)
    {
        CHECK_FALSE(guides.guides[i].outer);
        CHECK(guides.guides[i].node == 2);
    }
    CHECK(guides.inner_node == 2);
    CHECK(count_zone(guides, DropZone::Centre, false) == 1);
}

TEST_CASE("drop_guides: the compass shrinks with the node and never disappears")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    for (float width = 60.0f; width <= 900.0f; width += 7.0f)
    {
        for (float height : { 60.0f, 130.0f, 600.0f })
        {
            const Rect surface{ Vec2f(10.0f, 20.0f), Vec2f(width, height) };
            const SolvedLayout solved = solve(layout, panels, k_metrics, surface);
            const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(surface));
            CHECK(guides.count == 9);
            const float expected = math::clamp(math::min(width, height) / 8.0f, 0.5f * k_metrics.guide_unit, 0.875f * k_metrics.guide_unit);
            CHECK(guides.half == doctest::Approx(std::trunc(expected)));
        }
    }
}

TEST_CASE("drop_guides: none outside the surface, with guides off, or for a panel that may not dock")
{
    PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    CHECK(drop_guides(layout, panels, solved, pid("a"), Vec2f(-5.0f, 10.0f)).count == 0);

    DockMetrics off = k_metrics;
    off.style.guides = false;
    const SolvedLayout no_guides = solve(layout, panels, off, k_surface);
    CHECK(drop_guides(layout, panels, no_guides, pid("a"), centre(no_guides.nodes[2].body)).count == 0);

    set_flags(panels, "a", panel_flag::all & ~panel_flag::dock_elsewhere);
    solved = solve(layout, panels, k_metrics, k_surface);
    CHECK(drop_guides(layout, panels, solved, pid("a"), centre(solved.nodes[2].body)).count == 0);
}

TEST_CASE("drop_guides: allowed and reason equal can_dock_into for every flag set and dock rule")
{
    for (uint32_t bits = 0; bits < 64; ++bits)
    {
        for (uint32_t rule = 0; rule < 3; ++rule)
        {
            PanelTable panels = make_panels();
            const DockLayout layout = make_sample(panels);
            set_flags(panels, "b", static_cast<uint8_t>(bits));
            if (rule == 1)
                REQUIRE(add_dock_only(panels, pid("b"), pid("vp")));
            if (rule == 2)
                REQUIRE(add_dock_never(panels, pid("b"), pid("a")));
            const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
            for (int32_t hover : { 1, 2 })
            {
                const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[hover].body));
                CHECK((guides.count == 0) == (can_dock(panels, pid("b")) != DockReason::None));
                for (uint32_t i = 0; i < guides.count; ++i)
                {
                    const DockReason expected = can_dock_into(layout, panels, pid("b"), guides.guides[i].node);
                    CHECK(guides.guides[i].reason == expected);
                    CHECK(guides.guides[i].allowed == (expected == DockReason::None));
                }
            }
        }
    }
}

TEST_CASE("drop_guides: golden compass of the sample layout")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), centre(solved.nodes[2].body));
    std::string text;
    for (uint32_t i = 0; i < guides.count; ++i)
    {
        const DropGuide& guide = guides.guides[i];
        char line[160];
        std::snprintf(line, sizeof(line), "%d z%d %.1f,%.1f %.1fx%.1f %s\n", guide.node, static_cast<int>(guide.zone), guide.rect.min[0], guide.rect.min[1], guide.rect.size[0], guide.rect.size[1], guide.allowed ? "ok" : to_string(guide.reason));
        text += line;
    }
    CHECK_MESSAGE(text == "-1 z1 6.0,286.0 28.0x28.0 ok\n"
                    "-1 z2 766.0,286.0 28.0x28.0 ok\n"
                    "-1 z3 386.0,6.0 28.0x28.0 ok\n"
                    "-1 z4 386.0,566.0 28.0x28.0 ok\n"
                    "2 z1 634.0,286.0 28.0x28.0 ok\n"
                    "2 z2 698.0,286.0 28.0x28.0 ok\n"
                    "2 z3 666.0,254.0 28.0x28.0 ok\n"
                    "2 z4 666.0,318.0 28.0x28.0 ok\n"
                    "2 z0 666.0,286.0 28.0x28.0 ok\n", text);
}

TEST_CASE("drop_guides: a floating panel gets the centre guide too")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(float_panel(layout, panels, pid("b"), Rect{ Vec2f(100.0f, 100.0f), Vec2f(200.0f, 150.0f) }));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[2].body));
    CHECK(guides.count == 9);
    CHECK(count_zone(guides, DropZone::Centre, false) == 1);
}

TEST_CASE("resolve_drop: a pointer over a guide plans exactly that zone, and applying it equals dock_panel")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[2].body));
    REQUIRE(guides.count == 9);
    for (uint32_t i = 0; i < guides.count; ++i)
    {
        const DropGuide& guide = guides.guides[i];
        const DropPlan plan = resolve_drop(layout, panels, solved, pid("b"), centre(guide.rect), false);
        REQUIRE(plan.action == DropAction::Dock);
        CHECK(plan.guide == static_cast<int32_t>(i));
        CHECK(plan.node == guide.node);
        CHECK(plan.zone == guide.zone);
        CHECK(within(solved.surface_rect, plan.preview));

        DockLayout by_plan = layout;
        DockLayout by_call = layout;
        require_applied(dock_panel(by_plan, panels, pid("b"), plan.node, plan.zone));
        require_applied(dock_panel(by_call, panels, pid("b"), guide.node, guide.zone));
        CHECK(equal(by_plan, by_call));
    }
}

TEST_CASE("guide_at: circling the centre selects each side in turn and the centre inside, with no gaps")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[2].body));
    REQUIRE(guides.half > 0.0f);
    const float r = guides.half * 2.0f;
    const struct { Vec2f delta; DropZone zone; } probes[] = {
        { Vec2f(-r, 0.0f), DropZone::Left }, { Vec2f(r, 0.0f), DropZone::Right }, { Vec2f(0.0f, -r), DropZone::Top }, { Vec2f(0.0f, r), DropZone::Bottom },
        { Vec2f(-r, -r * 0.8f), DropZone::Left }, { Vec2f(r * 0.8f, -r), DropZone::Top }, { Vec2f(r, r * 0.6f), DropZone::Right }, { Vec2f(-r * 0.7f, r), DropZone::Bottom },
        { Vec2f(guides.half * 0.5f, guides.half * 0.5f), DropZone::Centre },
    };
    for (const auto& probe : probes)
    {
        const int32_t index = guide_at(guides, guides.centre + probe.delta);
        REQUIRE(index >= 0);
        CHECK(guides.guides[index].zone == probe.zone);
        CHECK(guides.guides[index].node == guides.inner_node);
    }
    CHECK(guide_at(guides, guides.centre + Vec2f(0.0f, guides.half * 5.0f)) == -1);
}

TEST_CASE("resolve_drop: a refused guide stays and carries the reason, and the rest of the body plans nothing")
{
    PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    REQUIRE(add_dock_never(panels, pid("b"), pid("vp")));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[2].body));
    REQUIRE(guides.count == 9);
    uint32_t refused_nodes = 0;
    for (uint32_t i = 0; i < guides.count; ++i)
    {
        const DropGuide& guide = guides.guides[i];
        if (guide.node == 2)
        {
            ++refused_nodes;
            CHECK_FALSE(guide.allowed);
            CHECK(guide.reason == DockReason::NotPermittedTarget);
            const DropPlan plan = resolve_drop(layout, panels, solved, pid("b"), centre(guide.rect), false);
            CHECK(plan.action == DropAction::Cancel);
            CHECK(plan.reason == DockReason::NotPermittedTarget);
            CHECK(plan.guide == static_cast<int32_t>(i));
            CHECK(is_empty(plan.preview));
        }
    }
    CHECK(refused_nodes == 5);

    const DropPlan elsewhere = resolve_drop(layout, panels, solved, pid("b"), Vec2f(120.0f, 300.0f), false);
    CHECK(elsewhere.guide == -1);
    CHECK(elsewhere.action == DropAction::None);
}

TEST_CASE("drop_guides: a drop that leaves the arrangement as it is is drawn as a here guide,")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);

    // vp already sits alone at the far right: its own node and the outer Right guide are "here".
    const DropGuides own = drop_guides(layout, panels, solved, pid("vp"), centre(solved.nodes[2].body));
    REQUIRE(own.count == 9);
    for (uint32_t i = 0; i < own.count; ++i)
    {
        const DropGuide& guide = own.guides[i];
        CHECK(guide.here == (!guide.outer || guide.zone == DropZone::Right));
        CHECK(guide.outer == (guide.node == k_dock_root));
    }

    const DropPlan band = resolve_drop(layout, panels, solved, pid("vp"), Vec2f(795.0f, 300.0f), false);
    CHECK(band.action == DropAction::None);
    for (uint32_t i = 0; i < own.count; ++i)
    {
        if (!own.guides[i].here)
            continue;
        const DropPlan over = resolve_drop(layout, panels, solved, pid("vp"), centre(own.guides[i].rect), false);
        CHECK(over.action == DropAction::None);
        CHECK(over.reason == DockReason::None);
        CHECK(over.guide == static_cast<int32_t>(i));
        CHECK(is_empty(over.preview));
    }

    // A tab of a two-tab node may still split off from its own node; dropping it back into the centre is "here".
    const DropGuides tabbed = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[1].body));
    REQUIRE(count_zone(tabbed, DropZone::Left, false) == 1);
    REQUIRE(count_zone(tabbed, DropZone::Centre, false) == 1);
    CHECK_FALSE(tabbed.guides[guide_index(tabbed, 1, DropZone::Left)].here);
    CHECK(tabbed.guides[guide_index(tabbed, 1, DropZone::Centre)].here);
}

TEST_CASE("drop_guides: the board beside a lone panel marks the Right drop for that panel as here")
{
    PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Right));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const int32_t board = node_of(layout, pid("vp"));
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), centre(solved.nodes[board].body));
    const int32_t right = guide_index(guides, board, DropZone::Right);
    const int32_t outer_right = guide_index(guides, k_dock_root, DropZone::Right);
    const int32_t left = guide_index(guides, board, DropZone::Left);
    REQUIRE(right >= 0);
    REQUIRE(outer_right >= 0);
    REQUIRE(left >= 0);
    CHECK(guides.guides[right].here);
    CHECK(guides.guides[outer_right].here);
    CHECK_FALSE(guides.guides[left].here);
}

TEST_CASE("drop_guides: a single stack still offers all four outer guides")
{
    const PanelTable panels = make_panels();
    DockLayout one;
    require_applied(dock_panel(one, panels, pid("a"), k_dock_root, DropZone::Centre));
    const SolvedLayout single = solve(one, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(one, panels, single, pid("b"), centre(k_surface));
    CHECK(guides.count == 9);
    uint32_t bars = 0;
    for (uint32_t i = 0; i < guides.count; ++i)
        bars += guides.guides[i].outer ? 1u : 0u;
    CHECK(bars == 4);
    check_well_formed(guides, single);
}

TEST_CASE("resolve_drop: an outer drop previews a sidebar of dock_size points and applying it fixes that size")
{
    PanelTable panels = make_panels();
    find_panel(panels, pid("a"))->dock_size = 120.0f;
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), centre(solved.nodes[2].body));
    for (const DropZone zone : { DropZone::Left, DropZone::Right, DropZone::Top, DropZone::Bottom })
    {
        const int32_t index = guide_index(guides, k_dock_root, zone);
        REQUIRE(index >= 0);
        const DropPlan plan = resolve_drop(layout, panels, solved, pid("a"), centre(guides.guides[index].rect), false);
        REQUIRE(plan.action == DropAction::Dock);
        const bool sideways = zone == DropZone::Left || zone == DropZone::Right;
        CHECK(plan.preview.size[sideways ? 0 : 1] == doctest::Approx(120.0f));
        CHECK(plan.preview.size[sideways ? 1 : 0] == doctest::Approx(k_surface.size[sideways ? 1 : 0]));

        DockLayout applied = layout;
        require_applied(dock_panel(applied, panels, pid("a"), plan.node, plan.zone));
        const int32_t root = applied.roots[0];
        CHECK(applied.nodes[root].mode == (zone == DropZone::Left || zone == DropZone::Top ? DockSizeMode::FixedFirst : DockSizeMode::FixedSecond));
        CHECK(applied.nodes[root].points == doctest::Approx(120.0f));
    }

    const DropPlan beside = resolve_drop(layout, panels, solved, pid("a"), Vec2f(795.0f, 300.0f), false);
    CHECK(beside.action == DropAction::None);
}

TEST_CASE("resolve_drop: a float under the pointer is neutral, not the node hidden beneath it")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(float_panel(layout, panels, pid("b"), Rect{ Vec2f(100.0f, 100.0f), Vec2f(200.0f, 150.0f) }));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const Vec2f over_float = centre(solved.floats[0]);
    const DropPlan plan = resolve_drop(layout, panels, solved, pid("a"), over_float, false);
    CHECK(plan.action == DropAction::None);
    CHECK(is_empty(plan.preview));
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), over_float);
    for (uint32_t i = 0; i < guides.count; ++i)
        CHECK(guides.guides[i].outer);
}

TEST_CASE("describe: every reason has a human sentence and the full-pool reasons differ from the developer wording")
{
    for (uint32_t r = 0; r <= static_cast<uint32_t>(DockReason::Collapsed); ++r)
    {
        const DockReason reason = static_cast<DockReason>(r);
        if (reason != DockReason::None)
            CHECK(std::strlen(describe(reason)) > 0);
    }
    for (const DockReason reason : { DockReason::TabsFull, DockReason::NodesFull, DockReason::FloatsFull })
        CHECK(std::string_view(describe(reason)) != std::string_view(to_string(reason)));
    CHECK(std::strlen(describe(DockReason::TargetInvalid)) > 0);
}

namespace
{

struct GridLayout
{
    const char* name;
    DockLayout layout;
};

std::vector<GridLayout> grid_layouts(const PanelTable& panels)
{
    std::vector<GridLayout> out;
    out.push_back({ "sample", make_sample(panels) });

    DockLayout single;
    require_applied(dock_panel(single, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(single, panels, pid("b"), k_dock_root, DropZone::Centre));
    out.push_back({ "single stack", single });

    DockLayout narrow = make_sample(panels);
    require_applied(dock_panel(narrow, panels, pid("c"), k_dock_root, DropZone::Left));
    for (uint32_t n = 0; n < narrow.node_count; ++n)
        if (narrow.nodes[n].kind == DockNodeKind::Split)
            require_applied(set_split(narrow, panels, static_cast<int32_t>(n), DockSizeMode::Ratio, 0.05f, 0.0f));
    out.push_back({ "narrow column", narrow });

    DockLayout collapsed = make_sample(panels);
    require_applied(dock_panel(collapsed, panels, pid("d"), k_dock_root, DropZone::Bottom));
    for (uint32_t n = 0; n < collapsed.node_count; ++n)
        if (collapsed.nodes[n].kind == DockNodeKind::Tabs && collapsed.nodes[n].tabs[0] == pid("d"))
            require_applied(set_collapsed(collapsed, panels, static_cast<int32_t>(n), true));
    out.push_back({ "collapsed row", collapsed });

    DockLayout floated = make_sample(panels);
    require_applied(float_panel(floated, panels, pid("a"), Rect{ Vec2f(150.0f, 150.0f), Vec2f(220.0f, 160.0f) }));
    out.push_back({ "with float", floated });
    return out;
}

bool over_strip(const SolvedLayout& solved, const DockLayout& layout, const Vec2f& pointer)
{
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && contains(solved.nodes[n].strip, pointer))
            return true;
    return false;
}

} // namespace

TEST_CASE("resolve_drop: nothing docks or reorders unless a guide or a tab strip is under the pointer")
{
    const PanelTable panels = make_panels();
    for (const GridLayout& entry : grid_layouts(panels))
    {
        const SolvedLayout solved = solve(entry.layout, panels, k_metrics, k_surface);
        for (const char* dragged : { "b", "vp" })
        {
            if (!is_open(entry.layout, pid(dragged)))
                continue;
            for (float y = 1.0f; y < k_surface.size[1]; y += 9.0f)
                for (float x = 1.0f; x < k_surface.size[0]; x += 9.0f)
                {
                    const Vec2f p(x, y);
                    const DropGuides guides = drop_guides(entry.layout, panels, solved, pid(dragged), p);
                    const DropPlan plan = resolve_drop(entry.layout, panels, solved, pid(dragged), p, false);
                    if (plan.action == DropAction::Dock || plan.action == DropAction::Reorder)
                        CHECK_MESSAGE((guide_at(guides, p) >= 0 || over_strip(solved, entry.layout, p)), entry.name << " " << dragged << " at " << x << "," << y);
                }
        }
    }
}

TEST_CASE("drop_guides: every leaf gets 5 inner and every pointer inside the surface gets 4 outer guides")
{
    const PanelTable panels = make_panels();
    for (const GridLayout& entry : grid_layouts(panels))
    {
        const SolvedLayout solved = solve(entry.layout, panels, k_metrics, k_surface);
        for (float y = 1.0f; y < k_surface.size[1]; y += 13.0f)
            for (float x = 1.0f; x < k_surface.size[0]; x += 13.0f)
            {
                const Vec2f p(x, y);
                const DropGuides guides = drop_guides(entry.layout, panels, solved, pid("e"), p);
                bool over_float = false;
                for (uint32_t f = 0; f < entry.layout.float_count; ++f)
                    over_float = over_float || contains(solved.floats[f], p);
                bool in_leaf = false;
                for (uint32_t n = 0; n < entry.layout.node_count; ++n)
                    in_leaf = in_leaf || (entry.layout.nodes[n].kind == DockNodeKind::Tabs && contains(solved.nodes[n].rect, p));
                uint32_t outer = 0;
                uint32_t inner = 0;
                for (uint32_t i = 0; i < guides.count; ++i)
                    (guides.guides[i].outer ? outer : inner) += 1u;
                CHECK_MESSAGE(outer == 4u, entry.name << " outer at " << x << "," << y);
                CHECK_MESSAGE(inner == (in_leaf && !over_float ? 5u : 0u), entry.name << " inner at " << x << "," << y);
            }
    }
}
