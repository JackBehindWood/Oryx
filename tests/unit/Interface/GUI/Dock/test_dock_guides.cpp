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
    for (uint32_t i = 0; i < guides.count; ++i)
        if (guides.guides[i].node != k_dock_root)
            CHECK(guides.guides[i].node == 2);
    CHECK(count_zone(guides, DropZone::Centre, false) == 1);
}

TEST_CASE("drop_guides: small nodes keep the centre guide, then nothing, and nothing ever overlaps")
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
            check_well_formed(guides, solved);
            CHECK((guides.count == 0 || guides.count == 5 || guides.count == 9));
        }
    }

    const SolvedLayout wide = solve(layout, panels, k_metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(800.0f, 600.0f) });
    CHECK(drop_guides(layout, panels, wide, pid("b"), centre(wide.surface_rect)).count == 9);
    const float cluster = 3.0f * k_metrics.guide_size + 2.0f * k_metrics.guide_gap;
    const float band = k_metrics.guide_inset + k_metrics.guide_size;
    const Rect tight{ Vec2f(0.0f, 0.0f), Vec2f(2.0f * band + cluster - 4.0f, 2.0f * band + cluster - 4.0f) };
    const SolvedLayout small = solve(layout, panels, k_metrics, tight);
    const DropGuides centre_only = drop_guides(layout, panels, small, pid("b"), centre(tight));
    CHECK(centre_only.count == 5);
    CHECK(count_zone(centre_only, DropZone::Centre, false) == 1);
    CHECK(count_zone(centre_only, DropZone::Left, false) == 0);
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

TEST_CASE("drop_guides: a floating panel gets no centre guide")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(float_panel(layout, panels, pid("b"), Rect{ Vec2f(100.0f, 100.0f), Vec2f(200.0f, 150.0f) }));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[2].body));
    CHECK(guides.count == 8);
    CHECK(count_zone(guides, DropZone::Centre, false) == 0);
}

TEST_CASE("resolve_drop: a pointer over a guide plans exactly that zone, and applying it equals dock_panel")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const Vec2f hover = centre(solved.nodes[2].body);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("b"), hover);
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

TEST_CASE("resolve_drop: a guide beats the edge band it sits in")
{
    PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), k_dock_root, DropZone::Right));
    require_applied(dock_panel(layout, panels, pid("d"), k_dock_root, DropZone::Right));
    const int32_t inner = layout.nodes[layout.roots[0]].first;
    require_applied(set_split(layout, panels, layout.roots[0], DockSizeMode::Ratio, 0.5f, 0.0f));
    require_applied(set_split(layout, panels, inner, DockSizeMode::Ratio, 0.88f, 0.0f));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const int32_t narrow = node_of(layout, pid("c"));
    const Rect& body = solved.nodes[narrow].body;
    REQUIRE(body.size[0] < 2.0f * k_metrics.guide_size + 2.0f);

    const Vec2f mid = centre(body);
    const DropGuides guides = drop_guides(layout, panels, solved, pid("d"), mid);
    REQUIRE(count_zone(guides, DropZone::Centre, false) == 1);
    uint32_t centre_index = 0;
    for (uint32_t i = 0; i < guides.count; ++i)
        if (guides.guides[i].node == narrow && guides.guides[i].zone == DropZone::Centre)
            centre_index = i;
    const Rect& g = guides.guides[centre_index].rect;
    const Vec2f in_band(g.min[0] + 1.0f, mid[1]);
    const float u = (in_band[0] - body.min[0]) / body.size[0];
    REQUIRE(u < k_metrics.edge_band);

    const DropTarget band = drop_target(layout, solved, in_band);
    CHECK(band.zone == DropZone::Left);
    const DropPlan plan = resolve_drop(layout, panels, solved, pid("d"), in_band, false);
    REQUIRE(plan.action == DropAction::Dock);
    CHECK(plan.zone == DropZone::Centre);
    CHECK(plan.node == narrow);
    CHECK(plan.guide == static_cast<int32_t>(centre_index));
}

TEST_CASE("resolve_drop: a refused guide stays, carries the reason, and the bands remain the fallback")
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

    const DropPlan fallback = resolve_drop(layout, panels, solved, pid("b"), Vec2f(40.0f, 300.0f), false);
    CHECK(fallback.guide == -1);
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
                    "2 z1 617.6,286.0 28.0x28.0 ok\n"
                    "2 z2 681.6,286.0 28.0x28.0 ok\n"
                    "2 z3 649.6,254.0 28.0x28.0 ok\n"
                    "2 z4 649.6,318.0 28.0x28.0 ok\n"
                    "2 z0 649.6,286.0 28.0x28.0 ok\n", text);
}

TEST_CASE("drop_guides: no guide that would leave the arrangement as it is, for guides and bands alike")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);

    // vp already sits alone at the far right: nothing of its own node and no outer Right guide is offered.
    const DropGuides own = drop_guides(layout, panels, solved, pid("vp"), centre(solved.nodes[2].body));
    CHECK(own.count == 3);
    CHECK(count_zone(own, DropZone::Right, true) == 0);
    for (uint32_t i = 0; i < own.count; ++i)
        CHECK(own.guides[i].node == k_dock_root);

    const DropPlan band = resolve_drop(layout, panels, solved, pid("vp"), Vec2f(795.0f, 300.0f), false);
    CHECK(band.action == DropAction::None);
    const DropPlan own_centre = resolve_drop(layout, panels, solved, pid("vp"), centre(solved.nodes[2].body), false);
    CHECK(own_centre.action == DropAction::None);

    // A tab of a two-tab node may still split off from its own node.
    const DropGuides tabbed = drop_guides(layout, panels, solved, pid("b"), centre(solved.nodes[1].body));
    CHECK(count_zone(tabbed, DropZone::Left, false) == 1);
    CHECK(count_zone(tabbed, DropZone::Centre, false) == 0);
}

TEST_CASE("drop_guides: the board beside a lone panel offers no Right drop for that panel")
{
    PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Right));
    const SolvedLayout solved = solve(layout, panels, k_metrics, k_surface);
    const int32_t board = node_of(layout, pid("vp"));
    const DropGuides guides = drop_guides(layout, panels, solved, pid("a"), centre(solved.nodes[board].body));
    for (uint32_t i = 0; i < guides.count; ++i)
        CHECK_FALSE((guides.guides[i].node == board && guides.guides[i].zone == DropZone::Right));
    CHECK(count_zone(guides, DropZone::Left, false) == 1);
    CHECK(count_zone(guides, DropZone::Right, true) == 0);
}
