#include "unit/Interface/GUI/Dock/DockTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

const Rect k_rect{ Vec2f(20.0f, 30.0f), Vec2f(160.0f, 100.0f) };

std::string named(const DockLayout& layout, const PanelTable& panels) { return dump(layout, panels); }

}

TEST_CASE("dock_panel: the first panel creates the root")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [a*]\n");
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("dock_panel: centre appends and selects")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), 0, DropZone::Centre));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [a b c*]\n");
}

TEST_CASE("dock_panel: sample layout golden")
{
    const PanelTable panels = make_panels();
    const DockLayout layout = make_sample(panels);
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a b*]\n    tabs [vp*]\n");
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("dock_panel: every edge splits the target with the panel on that side")
{
    const PanelTable panels = make_panels();
    struct Case
    {
        DropZone zone;
        const char* expected;
    };
    const Case cases[] = {
        { DropZone::Left, "dock v1\nsurface 0\n  split h ratio 0.300\n    tabs [c*]\n    tabs [a b*]\n" },
        { DropZone::Right, "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a b*]\n    tabs [c*]\n" },
        { DropZone::Top, "dock v1\nsurface 0\n  split v ratio 0.300\n    tabs [c*]\n    tabs [a b*]\n" },
        { DropZone::Bottom, "dock v1\nsurface 0\n  split v ratio 0.700\n    tabs [a b*]\n    tabs [c*]\n" },
    };
    for (const Case& test_case : cases)
    {
        DockLayout layout;
        require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
        require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
        require_applied(dock_panel(layout, panels, pid("c"), 0, test_case.zone));
        CHECK(named(layout, panels) == test_case.expected);
        CHECK_NOTHROW(validate(layout));
    }
}

TEST_CASE("dock_panel: an edge of the root wraps the whole tree")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("c"), k_dock_root, DropZone::Top));
    CHECK(named(layout, panels) ==
          "dock v1\nsurface 0\n  split v ratio 0.300\n    tabs [c*]\n    split h ratio 0.700\n      tabs [a b*]\n      tabs [vp*]\n");
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("dock_panel: moving the last tab out collapses its split")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("vp"), 1, DropZone::Centre));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [a b vp*]\n");
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("dock_panel: splitting a node with the panel's old host emptied keeps the target")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("vp"), 1, DropZone::Bottom));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split v ratio 0.700\n    tabs [a b*]\n    tabs [vp*]\n");
}

TEST_CASE("dock_panel: same host centre moves the tab to the end")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(dock_panel(layout, panels, pid("a"), 1, DropZone::Centre));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [b a*]\n    tabs [vp*]\n");

    const DockLayout before = layout;
    const DockResult again = dock_panel(layout, panels, pid("a"), 1, DropZone::Centre);
    CHECK(again.reason == DockReason::NoChange);
    CHECK(equal(before, layout));
}

TEST_CASE("dock_panel: splitting a single-tab host with its own tab is no change")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    const DockLayout before = layout;
    CHECK(dock_panel(layout, panels, pid("vp"), 2, DropZone::Left).reason == DockReason::NoChange);
    CHECK(dock_panel(layout, panels, pid("vp"), k_dock_root, DropZone::Centre).reason == DockReason::TargetInvalid);
    CHECK(equal(before, layout));
}

TEST_CASE("dock_panel: invalid targets are refused untouched")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    const DockLayout before = layout;
    CHECK(dock_panel(layout, panels, pid("c"), 0, DropZone::Centre).reason == DockReason::TargetInvalid);
    CHECK(dock_panel(layout, panels, pid("c"), 0, DropZone::Left).reason == DockReason::TargetInvalid);
    CHECK(dock_panel(layout, panels, pid("c"), 99, DropZone::Centre).reason == DockReason::TargetInvalid);
    CHECK(dock_panel(layout, panels, pid("c"), -5, DropZone::Centre).reason == DockReason::TargetInvalid);
    CHECK(dock_panel(layout, panels, pid("c"), k_dock_root, DropZone::Centre).reason == DockReason::TargetInvalid);
    CHECK(equal(before, layout));
}

TEST_CASE("dock_panel: a full tab stack refuses and leaves the layout untouched")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    const char* names[] = { "a", "b", "c", "d", "e", "f", "g", "h" };
    for (const char* name : names)
        require_applied(dock_panel(layout, panels, pid(name), k_dock_root, DropZone::Centre));
    CHECK(layout.nodes[0].count == k_max_dock_tabs);

    const DockLayout before = layout;
    CHECK(dock_panel(layout, panels, pid("i"), 0, DropZone::Centre).reason == DockReason::TabsFull);
    CHECK(equal(before, layout));
    require_applied(dock_panel(layout, panels, pid("i"), 0, DropZone::Right));
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("dock_panel: 32 panels in 32 stacks fit the node pool and a lone panel can move")
{
    PanelTable panels;
    for (uint32_t i = 0; i < k_max_panels; ++i)
        REQUIRE(add_panel(panels, "n" + std::to_string(i), "t", PanelKind::View));
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("n0"), k_dock_root, DropZone::Centre));
    for (uint32_t i = 1; i < k_max_panels; ++i)
        require_applied(dock_panel(layout, panels, pid("n" + std::to_string(i)), k_dock_root, DropZone::Right));
    CHECK(layout.node_count == 2 * k_max_panels - 1);
    CHECK_NOTHROW(validate(layout));

    require_applied(dock_panel(layout, panels, pid("n0"), k_dock_root, DropZone::Top));
    CHECK(layout.node_count == 2 * k_max_panels - 1);
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("dock_panel: a pinned panel reorders in its host but cannot change host")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(open_panel(layout, panels, pid("p")));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a b p*]\n    tabs [vp*]\n");

    const DockLayout before = layout;
    CHECK(dock_panel(layout, panels, pid("p"), 2, DropZone::Centre).reason == DockReason::NotPermittedDock);
    CHECK(float_panel(layout, panels, pid("p"), k_rect).reason == DockReason::NotPermittedFloat);
    CHECK(equal(before, layout));

    require_applied(reorder_tab(layout, panels, pid("p"), 0));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [p* a b]\n    tabs [vp*]\n");
}

TEST_CASE("float_panel: floats, updates, redocks and refuses")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(float_panel(layout, panels, pid("b"), k_rect));
    CHECK(layout.float_count == 1);
    CHECK(named(layout, panels) ==
          "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a*]\n    tabs [vp*]\nfloat b surface 0 rect 20.000 30.000 160.000 100.000\n");

    CHECK(float_panel(layout, panels, pid("b"), k_rect).reason == DockReason::NoChange);
    const Rect moved{ Vec2f(50.0f, 60.0f), Vec2f(160.0f, 100.0f) };
    require_applied(float_panel(layout, panels, pid("b"), moved));
    CHECK(layout.floats[0].rect == moved);

    require_applied(dock_panel(layout, panels, pid("b"), 1, DropZone::Centre));
    CHECK(layout.float_count == 0);
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a b*]\n    tabs [vp*]\n");

    const DockLayout before = layout;
    CHECK(float_panel(layout, panels, pid("b"), Rect{ Vec2f(0.0f, 0.0f), Vec2f(0.0f, 10.0f) }).reason == DockReason::BadArgument);
    CHECK(float_panel(layout, panels, pid("b"), Rect{ Vec2f(0.0f, 0.0f), Vec2f(std::nanf(""), 10.0f) }).reason == DockReason::BadArgument);
    CHECK(equal(before, layout));
}

TEST_CASE("float_panel: the float pool is capped")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("j"), k_dock_root, DropZone::Centre));
    const char* names[] = { "a", "b", "c", "d", "e", "f", "g", "h" };
    for (const char* name : names)
        require_applied(float_panel(layout, panels, pid(name), k_rect));
    CHECK(layout.float_count == k_max_dock_floats);

    const DockLayout before = layout;
    CHECK(float_panel(layout, panels, pid("i"), k_rect).reason == DockReason::FloatsFull);
    CHECK(equal(before, layout));
    CHECK_NOTHROW(validate(layout));
}

TEST_CASE("close_panel and open_panel: reopen at the recorded home")
{
    const PanelTable panels = make_panels();
    const DockLayout original = make_sample(panels);

    DockLayout layout = original;
    require_applied(close_panel(layout, panels, pid("vp")));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [a b*]\nhome vp sibling a right\nclosed vp\n");
    require_applied(open_panel(layout, panels, pid("vp")));
    CHECK(equal(layout, original));

    require_applied(close_panel(layout, panels, pid("b")));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a*]\n    tabs [vp*]\nhome b sibling a centre\nclosed b\n");
    require_applied(open_panel(layout, panels, pid("b")));
    CHECK(equal(layout, original));
}

TEST_CASE("open_panel: falls back when the home sibling is gone")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    require_applied(close_panel(layout, panels, pid("b")));
    require_applied(close_panel(layout, panels, pid("a")));
    CHECK(layout.roots[0] == k_no_node);

    require_applied(open_panel(layout, panels, pid("b")));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [b*]\nhome a sibling - centre\nclosed a\n");
}

TEST_CASE("open_panel: a panel that was never placed joins the first tab stack")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(open_panel(layout, panels, pid("c")));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a b c*]\n    tabs [vp*]\n");
}

TEST_CASE("open_panel and close_panel: state refusals")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    const DockLayout before = layout;
    CHECK(open_panel(layout, panels, pid("a")).reason == DockReason::AlreadyOpen);
    CHECK(close_panel(layout, panels, pid("c")).reason == DockReason::AlreadyClosed);
    CHECK(close_panel(layout, panels, pid("vp")).reason == DockReason::NotPermittedClose);
    CHECK(equal(before, layout));

    require_applied(close_panel(layout, panels, pid("a")));
    CHECK(close_panel(layout, panels, pid("a")).reason == DockReason::AlreadyClosed);
}

TEST_CASE("open_panel: a floating panel is open, and closing it records a centre home")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(float_panel(layout, panels, pid("b"), k_rect));
    CHECK(open_panel(layout, panels, pid("b")).reason == DockReason::AlreadyOpen);
    require_applied(close_panel(layout, panels, pid("b")));
    CHECK(layout.float_count == 0);
    CHECK(layout.home_count == 1);
    CHECK_FALSE(is_valid(layout.homes[0].sibling));
}

TEST_CASE("open_panel: the home pool drops its oldest entry when full")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    layout.home_count = k_max_dock_homes;
    for (uint32_t i = 0; i < k_max_dock_homes; ++i)
        layout.homes[i].panel = PanelId{ 1000u + i };
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    require_applied(close_panel(layout, panels, pid("b")));
    CHECK(layout.home_count == k_max_dock_homes);
    CHECK(layout.homes[0].panel.hash == 1001u);
    CHECK(layout.homes[k_max_dock_homes - 1].panel == pid("b"));
}

TEST_CASE("reorder_tab: moves a tab and keeps the selection on the same panel")
{
    const PanelTable panels = make_panels();
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), 0, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("c"), 0, DropZone::Centre));
    require_applied(select_tab(layout, 0, 0));
    require_applied(reorder_tab(layout, panels, pid("c"), 0));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [c a* b]\n");
    require_applied(reorder_tab(layout, panels, pid("a"), 2));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  tabs [c b a*]\n");

    const DockLayout before = layout;
    CHECK(reorder_tab(layout, panels, pid("a"), 2).reason == DockReason::NoChange);
    CHECK(reorder_tab(layout, panels, pid("a"), 3).reason == DockReason::BadArgument);
    CHECK(reorder_tab(layout, panels, pid("d"), 0).reason == DockReason::NotFound);
    CHECK(equal(before, layout));
}

TEST_CASE("select_tab: selects, and refuses bad nodes and indices")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(select_tab(layout, 1, 0));
    CHECK(named(layout, panels) == "dock v1\nsurface 0\n  split h ratio 0.700\n    tabs [a* b]\n    tabs [vp*]\n");
    CHECK(select_tab(layout, 1, 0).reason == DockReason::NoChange);
    CHECK(select_tab(layout, 1, 2).reason == DockReason::BadArgument);
    CHECK(select_tab(layout, 0, 0).reason == DockReason::TargetInvalid);
    CHECK(select_tab(layout, 7, 0).reason == DockReason::TargetInvalid);
    CHECK(select_tab(layout, -1, 0).reason == DockReason::TargetInvalid);
}

TEST_CASE("set_collapsed: toggles, and a viewport tab refuses")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    require_applied(set_collapsed(layout, panels, 1, true));
    CHECK(named(layout, panels).find("tabs [a b*] collapsed") != std::string::npos);
    CHECK(set_collapsed(layout, panels, 1, true).reason == DockReason::NoChange);
    require_applied(set_collapsed(layout, panels, 1, false));

    CHECK(set_collapsed(layout, panels, 2, true).reason == DockReason::NotPermittedCollapse);
    CHECK(set_collapsed(layout, panels, 0, true).reason == DockReason::TargetInvalid);
}

TEST_CASE("set_split: clamps, validates and applies the either-side rule")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);

    require_applied(set_split(layout, panels, 0, DockSizeMode::Ratio, 2.0f, -5.0f));
    CHECK(layout.nodes[0].ratio == doctest::Approx(1.0f - k_dock_min_ratio));
    CHECK(layout.nodes[0].points == 0.0f);
    require_applied(set_split(layout, panels, 0, DockSizeMode::FixedSecond, 0.5f, 120.0f));
    CHECK(named(layout, panels).find("split h second 120.000") != std::string::npos);
    CHECK(set_split(layout, panels, 0, DockSizeMode::FixedSecond, 0.5f, 120.0f).reason == DockReason::NoChange);

    const DockLayout before = layout;
    CHECK(set_split(layout, panels, 0, DockSizeMode::Ratio, std::nanf(""), 0.0f).reason == DockReason::BadArgument);
    CHECK(set_split(layout, panels, 1, DockSizeMode::Ratio, 0.5f, 0.0f).reason == DockReason::TargetInvalid);
    CHECK(equal(before, layout));

    PanelTable locked = panels;
    set_flags(locked, "a", panel_flag::all & ~panel_flag::resize);
    require_applied(set_split(layout, locked, 0, DockSizeMode::Ratio, 0.4f, 0.0f));
    set_flags(locked, "b", panel_flag::all & ~panel_flag::resize);
    CHECK(set_split(layout, locked, 0, DockSizeMode::Ratio, 0.3f, 0.0f).reason == DockReason::NotPermittedResize);
}

TEST_CASE("equal: ignores unused slots and compares floats bitwise")
{
    const PanelTable panels = make_panels();
    DockLayout a = make_sample(panels);
    DockLayout b = a;
    b.nodes[1].tabs[5] = pid("j");
    b.nodes[40].ratio = 0.9f;
    CHECK(equal(a, b));
    b.nodes[0].ratio = 0.7001f;
    CHECK_FALSE(equal(a, b));
}

TEST_CASE("diff and dump: deterministic text")
{
    const PanelTable panels = make_panels();
    const DockLayout a = make_sample(panels);
    DockLayout b = a;
    CHECK(diff(a, b).empty());
    require_applied(select_tab(b, 1, 0));

    char first[16];
    char second[16];
    std::snprintf(first, sizeof(first), "#%08x", pid("a").hash);
    std::snprintf(second, sizeof(second), "#%08x", pid("b").hash);
    const std::string expected = std::string("-    tabs [") + first + " " + second + "*]\n+    tabs [" + first + "* " + second + "]\n";
    CHECK(diff(a, b) == expected);
    CHECK(dump(a) == dump(a));
    CHECK(dump(a) != dump(b));
}
