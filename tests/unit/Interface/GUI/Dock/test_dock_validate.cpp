#include "unit/Interface/GUI/Dock/DockTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

template<typename Mutate>
void check_corrupt(const PanelTable& panels, Mutate&& mutate)
{
    DockLayout layout = make_sample(panels);
    mutate(layout);
    CHECK_THROWS_AS(validate(layout), Error);
}

}

TEST_CASE("normalize: drops empty tabs and collapses their split")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    layout.nodes[2].count = 0;
    layout.nodes[2].tabs[0] = PanelId{};
    normalize(layout);
    CHECK(dump(layout, panels) == "dock v1\nsurface 0\n  tabs [a b*]\n");
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("normalize: an emptied root leaves an empty surface")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    layout.nodes[0].count = 0;
    normalize(layout);
    CHECK(layout.roots[0] == k_no_node);
    CHECK(layout.node_count == 0);
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("normalize: is idempotent and canonical")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), 2, DropZone::Bottom));
    require_applied(dock_panel(layout, panels, pid("d"), 1, DropZone::Left));
    DockLayout once = layout;
    normalize(once);
    CHECK(equal(once, layout));
    DockLayout twice = once;
    normalize(twice);
    CHECK(equal(twice, once));
}

TEST_CASE("normalize: clamps selected, ratio and points, and prunes stale records")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    layout.nodes[1].selected = 9;
    layout.nodes[0].ratio = 5.0f;
    layout.nodes[0].points = -3.0f;
    layout.closed[layout.closed_count++] = pid("a");
    layout.homes[layout.home_count++] = DockHome{ pid("a"), pid("b"), DropZone::Left };
    layout.homes[layout.home_count++] = DockHome{ pid("c"), pid("b"), DropZone::Left };
    normalize(layout);
    CHECK(layout.nodes[1].selected == 1);
    CHECK(layout.nodes[0].ratio == doctest::Approx(1.0f - k_dock_min_ratio));
    CHECK(layout.nodes[0].points == 0.0f);
    CHECK(layout.closed_count == 0);
    CHECK(layout.home_count == 1);
    CHECK(layout.homes[0].panel == pid("c"));
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("normalize: nested splits keep depth-first order")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), 2, DropZone::Bottom));
    CHECK(dump(layout, panels) ==
          "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a b*]\n    split v ratio 0.700\n      tabs [vp*]\n      tabs [c*]\n");
    CHECK(layout.nodes[3].kind == DockNodeKind::Tabs);
    CHECK(layout.nodes[4].kind == DockNodeKind::Tabs);
}

TEST_CASE("validate: accepts the layouts the operations build")
{
    const PanelTable panels = make_panels();
    DockLayout empty;
    CHECK_NOTHROW(validate(empty));
    CHECK_NOTHROW(validate(make_sample(panels)));
    CHECK_NOTHROW(validate(make_sample(panels), panels, ValidateFlags{ true }));
}

TEST_CASE("validate: throws oryx::Error for each corruption class")
{
    const PanelTable panels = make_panels();
    check_corrupt(panels, [](DockLayout& l) { l.version = 2; });
    check_corrupt(panels, [](DockLayout& l) { l.node_count = k_max_dock_nodes + 1; });
    check_corrupt(panels, [](DockLayout& l) { l.float_count = k_max_dock_floats + 1; });
    check_corrupt(panels, [](DockLayout& l) { l.home_count = k_max_dock_homes + 1; });
    check_corrupt(panels, [](DockLayout& l) { l.closed_count = k_max_dock_closed + 1; });
    check_corrupt(panels, [](DockLayout& l) { l.roots[0] = 99; });
    check_corrupt(panels, [](DockLayout& l) { l.roots[1] = -7; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].first = 99; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].second = -1; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].second = l.nodes[0].first; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[2] = l.nodes[0]; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].count = 0; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].count = static_cast<uint8_t>(k_max_dock_tabs + 1); });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].selected = 2; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].collapsed = 2; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].tabs[0] = PanelId{}; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].tabs[1] = l.nodes[1].tabs[0]; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].tabs[1] = l.nodes[2].tabs[0]; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].kind = DockNodeKind::Empty; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[1].kind = static_cast<DockNodeKind>(9); });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].axis = static_cast<DockAxis>(7); });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].mode = static_cast<DockSizeMode>(7); });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].ratio = std::nanf(""); });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].ratio = 1.5f; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].ratio = 0.0f; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].points = -1.0f; });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].points = std::numeric_limits<float>::infinity(); });
    check_corrupt(panels, [](DockLayout& l) { l.nodes[0].first = 0; });
    check_corrupt(panels, [](DockLayout& l) {
        l.nodes[l.node_count].kind = DockNodeKind::Tabs;
        l.nodes[l.node_count].count = 1;
        l.nodes[l.node_count].tabs[0] = pid("c");
        ++l.node_count;
    });
    check_corrupt(panels, [](DockLayout& l) { l.floats[l.float_count++] = DockFloat{ PanelId{}, 0, Rect{ Vec2f(0.0f, 0.0f), Vec2f(10.0f, 10.0f) } }; });
    check_corrupt(panels, [](DockLayout& l) { l.floats[l.float_count++] = DockFloat{ pid("c"), 5, Rect{ Vec2f(0.0f, 0.0f), Vec2f(10.0f, 10.0f) } }; });
    check_corrupt(panels, [](DockLayout& l) { l.floats[l.float_count++] = DockFloat{ pid("c"), 0, Rect{ Vec2f(0.0f, 0.0f), Vec2f(0.0f, 10.0f) } }; });
    check_corrupt(panels, [](DockLayout& l) { l.floats[l.float_count++] = DockFloat{ pid("c"), 0, Rect{ Vec2f(std::nanf(""), 0.0f), Vec2f(10.0f, 10.0f) } }; });
    check_corrupt(panels, [](DockLayout& l) { l.floats[l.float_count++] = DockFloat{ pid("a"), 0, Rect{ Vec2f(0.0f, 0.0f), Vec2f(10.0f, 10.0f) } }; });
    check_corrupt(panels, [](DockLayout& l) { l.homes[l.home_count++] = DockHome{ PanelId{}, PanelId{}, DropZone::Centre }; });
    check_corrupt(panels, [](DockLayout& l) { l.homes[l.home_count++] = DockHome{ pid("c"), PanelId{}, static_cast<DropZone>(9) }; });
    check_corrupt(panels, [](DockLayout& l) { l.closed[l.closed_count++] = PanelId{}; });
    check_corrupt(panels, [](DockLayout& l) { l.closed[l.closed_count++] = pid("a"); });
}

TEST_CASE("validate: the viewport requirement is the consumer's flag")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    CHECK_NOTHROW(validate(layout, panels, ValidateFlags{ false }));
    CHECK_THROWS_AS(validate(layout, panels, ValidateFlags{ true }), Error);

    DockLayout floating = layout;
    PanelTable tearable = panels;
    set_flags(tearable, "vp", panel_flag::all);
    require_applied(float_panel(floating, tearable, pid("vp"), Rect{ Vec2f(0.0f, 0.0f), Vec2f(50.0f, 50.0f) }));
    CHECK_NOTHROW(validate(floating, panels, ValidateFlags{ true }));

    require_applied(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Right));
    CHECK_NOTHROW(validate(layout, panels, ValidateFlags{ true }));
}

TEST_CASE("validate: reports a readable detail")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    layout.nodes[0].ratio = 9.0f;
    try
    {
        validate(layout);
        FAIL("validate accepted a bad ratio");
    }
    catch (const Error& error)
    {
        CHECK(std::string(error.what()).find("ratio") != std::string::npos);
    }
}

TEST_CASE("fuzz: mutated layouts either validate or throw oryx::Error, and survivors stay usable")
{
    const PanelTable panels = make_panels();
    const DockLayout base = make_sample(panels);
    const DockMetrics metrics;
    Lcg rng;
    uint32_t accepted = 0;
    uint32_t rejected = 0;

    for (uint32_t iteration = 0; iteration < 4000; ++iteration)
    {
        DockLayout layout = base;
        const uint32_t used_bytes = static_cast<uint32_t>(offsetof(DockLayout, nodes)) + 4u * static_cast<uint32_t>(sizeof(DockNode));
        uint8_t* bytes = reinterpret_cast<uint8_t*>(&layout);
        const uint32_t mutations = 1 + rng.below(4);
        for (uint32_t m = 0; m < mutations; ++m)
            bytes[rng.below(used_bytes)] = static_cast<uint8_t>(rng.below(256));

        bool valid = true;
        try
        {
            validate(layout);
        }
        catch (const Error&)
        {
            valid = false;
        }
        if (!valid)
        {
            ++rejected;
            continue;
        }
        ++accepted;

        const SolvedLayout solved = solve(layout, panels, metrics, Rect{ Vec2f(0.0f, 0.0f), Vec2f(400.0f, 300.0f) });
        for (uint32_t n = 0; n < solved.node_count; ++n)
        {
            CHECK(std::isfinite(solved.nodes[n].rect.size[0]));
            CHECK(std::isfinite(solved.nodes[n].rect.size[1]));
            CHECK(solved.nodes[n].rect.size[0] >= 0.0f);
            CHECK(solved.nodes[n].rect.size[1] >= 0.0f);
        }
        (void)strip_at(layout, solved, Vec2f(rng.unit() * 400.0f, rng.unit() * 300.0f));

        DockLayout copy = layout;
        normalize(copy);
        CHECK_NOTHROW(validate(copy));

        DockLayout worked = layout;
        (void)dock_panel(worked, panels, pid("c"), static_cast<int32_t>(rng.below(5)) - 1, static_cast<DropZone>(rng.below(5)));
        (void)close_panel(worked, panels, pid("a"));
        (void)open_panel(worked, panels, pid("a"));
        CHECK_NOTHROW(validate(worked));
    }
    CHECK(accepted > 0);
    CHECK(rejected > 0);
}

TEST_CASE("property: random operation sequences keep the layout valid and refusals untouched")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    Lcg rng;
    rng.state = 987654321u;
    uint32_t applied = 0;
    uint32_t refused = 0;

    for (uint32_t step = 0; step < 6000; ++step)
    {
        const PanelId panel = panels.descs[rng.below(panels.count)].id;
        const int32_t node = static_cast<int32_t>(rng.below(layout.node_count + 1)) - 1;
        const DockLayout before = layout;
        DockResult result;
        switch (rng.below(8))
        {
        case 0: result = dock_panel(layout, panels, panel, node, static_cast<DropZone>(rng.below(5))); break;
        case 1: result = float_panel(layout, panels, panel, Rect{ Vec2f(rng.unit() * 300.0f, rng.unit() * 200.0f), Vec2f(40.0f + rng.unit() * 200.0f, 30.0f + rng.unit() * 100.0f) }); break;
        case 2: result = close_panel(layout, panels, panel); break;
        case 3: result = open_panel(layout, panels, panel); break;
        case 4: result = reorder_tab(layout, panels, panel, rng.below(9)); break;
        case 5: result = select_tab(layout, node, rng.below(9)); break;
        case 6: result = set_collapsed(layout, panels, node, (rng.next() & 1u) != 0); break;
        default: result = set_split(layout, panels, node, static_cast<DockSizeMode>(rng.below(3)), rng.unit() * 1.2f - 0.1f, rng.unit() * 200.0f - 20.0f); break;
        }

        CAPTURE(step);
        CHECK(result.applied == (result.reason == DockReason::None));
        if (!result.applied)
        {
            ++refused;
            REQUIRE(equal(before, layout));
            continue;
        }
        ++applied;
        REQUIRE_NOTHROW(validate(layout));
        DockLayout canonical = layout;
        normalize(canonical);
        REQUIRE(equal(canonical, layout));
    }
    CHECK(applied > 500);
    CHECK(refused > 500);
}
