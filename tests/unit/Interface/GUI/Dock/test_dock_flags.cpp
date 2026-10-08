#include "unit/Interface/GUI/Dock/DockTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

const Rect k_float_rect{ Vec2f(10.0f, 10.0f), Vec2f(120.0f, 90.0f) };

DockLayout hosts_layout(const PanelTable& panels) { return make_sample(panels); }

DockLayout split_layout(const PanelTable& panels)
{
    DockLayout layout;
    require_applied(dock_panel(layout, panels, pid("a"), k_dock_root, DropZone::Centre));
    require_applied(dock_panel(layout, panels, pid("b"), k_dock_root, DropZone::Right));
    return layout;
}

struct MatrixOp
{
    const char* name;
    uint8_t bit;
    DockReason denied;
    DockLayout (*setup)(const PanelTable&);
    DockResult (*run)(DockLayout&, const PanelTable&);
};

const MatrixOp k_matrix[] = {
    { "reorder_tab", panel_flag::reorder_in_host, DockReason::NotPermittedReorder, hosts_layout, [](DockLayout& l, const PanelTable& p) { return reorder_tab(l, p, pid("a"), 1); } },
    { "dock_panel same host", panel_flag::reorder_in_host, DockReason::NotPermittedReorder, hosts_layout, [](DockLayout& l, const PanelTable& p) { return dock_panel(l, p, pid("a"), 1, DropZone::Centre); } },
    { "dock_panel other host", panel_flag::dock_elsewhere, DockReason::NotPermittedDock, hosts_layout, [](DockLayout& l, const PanelTable& p) { return dock_panel(l, p, pid("a"), 2, DropZone::Centre); } },
    { "dock_panel edge", panel_flag::dock_elsewhere, DockReason::NotPermittedDock, hosts_layout, [](DockLayout& l, const PanelTable& p) { return dock_panel(l, p, pid("a"), 2, DropZone::Bottom); } },
    { "float_panel", panel_flag::tear_off, DockReason::NotPermittedFloat, hosts_layout, [](DockLayout& l, const PanelTable& p) { return float_panel(l, p, pid("a"), k_float_rect); } },
    { "close_panel", panel_flag::close, DockReason::NotPermittedClose, hosts_layout, [](DockLayout& l, const PanelTable& p) { return close_panel(l, p, pid("a")); } },
    { "set_collapsed", panel_flag::collapse, DockReason::NotPermittedCollapse, hosts_layout, [](DockLayout& l, const PanelTable& p) { return set_collapsed(l, p, 1, true); } },
    { "set_split", panel_flag::resize, DockReason::NotPermittedResize, split_layout, [](DockLayout& l, const PanelTable& p) { return set_split(l, p, 0, DockSizeMode::Ratio, 0.4f, 0.0f); } },
};

}

TEST_CASE("PanelId: FNV-1a goldens")
{
    static_assert(make_panel_id("").hash == 0x811c9dc5u);
    static_assert(make_panel_id("a").hash == 0xe40c292cu);
    CHECK(make_panel_id("view/probabilities") == make_panel_id("view/probabilities"));
    CHECK(make_panel_id("view/probabilities") != make_panel_id("view/values"));
    CHECK(is_valid(make_panel_id("anything")));
    CHECK_FALSE(is_valid(PanelId{}));
}

TEST_CASE("PanelFlags: defaults and pinned")
{
    CHECK(default_flags(PanelKind::View).bits == panel_flag::all);
    const PanelFlags viewport = default_flags(PanelKind::Viewport);
    CHECK(has_flag(viewport, panel_flag::reorder_in_host));
    CHECK(has_flag(viewport, panel_flag::dock_elsewhere));
    CHECK(has_flag(viewport, panel_flag::resize));
    CHECK_FALSE(has_flag(viewport, panel_flag::collapse));
    CHECK_FALSE(has_flag(viewport, panel_flag::close));
    CHECK_FALSE(has_flag(viewport, panel_flag::tear_off));

    const PanelFlags pin = pinned(default_flags(PanelKind::View));
    CHECK_FALSE(has_flag(pin, panel_flag::dock_elsewhere));
    CHECK_FALSE(has_flag(pin, panel_flag::tear_off));
    CHECK(has_flag(pin, panel_flag::reorder_in_host));
    CHECK(has_flag(pin, panel_flag::resize));
}

TEST_CASE("PanelFlags: add_panel rejects duplicates and a full table")
{
    PanelTable table;
    CHECK(add_panel(table, "x", "X", PanelKind::View));
    CHECK_FALSE(add_panel(table, "x", "X again", PanelKind::View));
    for (uint32_t i = 1; i < k_max_panels; ++i)
        CHECK(add_panel(table, std::to_string(i), "t", PanelKind::View));
    CHECK_FALSE(add_panel(table, "overflow", "t", PanelKind::View));
}

TEST_CASE("PanelFlags matrix: all 64 combinations x every operation")
{
    for (uint32_t bits = 0; bits <= panel_flag::all; ++bits)
    {
        for (const MatrixOp& op : k_matrix)
        {
            CAPTURE(bits);
            CAPTURE(op.name);
            PanelTable full = make_panels();
            DockLayout layout = op.setup(full);

            PanelTable table = full;
            set_flags(table, "a", static_cast<uint8_t>(bits));
            const DockLayout before = layout;
            const DockResult result = op.run(layout, table);

            if ((bits & op.bit) != 0)
            {
                CHECK(result.applied);
                CHECK(result.reason == DockReason::None);
                CHECK_FALSE(equal(before, layout));
                CHECK_NOTHROW(validate(layout));
            }
            else
            {
                CHECK_FALSE(result.applied);
                CHECK(result.reason == op.denied);
                CHECK(equal(before, layout));
                CHECK(dump(before) == dump(layout));
            }
        }
    }
}

TEST_CASE("PanelFlags matrix: open and select never need a flag")
{
    for (uint32_t bits = 0; bits <= panel_flag::all; ++bits)
    {
        CAPTURE(bits);
        PanelTable full = make_panels();
        DockLayout layout = make_sample(full);
        require_applied(close_panel(layout, full, pid("a")));

        PanelTable table = full;
        set_flags(table, "a", static_cast<uint8_t>(bits));
        CHECK(open_panel(layout, table, pid("a")).applied);
        CHECK(select_tab(layout, 1, 0).applied);
    }
}

TEST_CASE("PanelFlags: can_* reports the matching refusal and UnknownPanel")
{
    PanelTable table = make_panels();
    set_flags(table, "a", 0);
    CHECK(can_reorder(table, pid("a")) == DockReason::NotPermittedReorder);
    CHECK(can_dock(table, pid("a")) == DockReason::NotPermittedDock);
    CHECK(can_float(table, pid("a")) == DockReason::NotPermittedFloat);
    CHECK(can_resize(table, pid("a")) == DockReason::NotPermittedResize);
    CHECK(can_collapse(table, pid("a")) == DockReason::NotPermittedCollapse);
    CHECK(can_close(table, pid("a")) == DockReason::NotPermittedClose);
    CHECK(can_close(table, pid("missing")) == DockReason::UnknownPanel);
    CHECK(can_close(table, pid("b")) == DockReason::None);
}

TEST_CASE("PanelFlags: operations on an unregistered panel are refused")
{
    const PanelTable panels = make_panels();
    DockLayout layout = make_sample(panels);
    const DockLayout before = layout;
    const PanelId ghost = pid("ghost");
    CHECK(dock_panel(layout, panels, ghost, 1, DropZone::Centre).reason == DockReason::UnknownPanel);
    CHECK(float_panel(layout, panels, ghost, k_float_rect).reason == DockReason::UnknownPanel);
    CHECK(close_panel(layout, panels, ghost).reason == DockReason::UnknownPanel);
    CHECK(open_panel(layout, panels, ghost).reason == DockReason::UnknownPanel);
    CHECK(reorder_tab(layout, panels, ghost, 0).reason == DockReason::UnknownPanel);
    CHECK(equal(before, layout));
}
