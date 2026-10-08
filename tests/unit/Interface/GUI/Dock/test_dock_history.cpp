#include "unit/Interface/GUI/Dock/DockTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

DockLayout with_ratio(const PanelTable& panels, float ratio)
{
    DockLayout layout = make_sample(panels);
    require_applied(set_split(layout, panels, 0, DockSizeMode::Ratio, ratio, 0.0f));
    return layout;
}

}

TEST_CASE("LayoutHistory: undo and redo walk the snapshots")
{
    const PanelTable panels = make_panels();
    auto history = std::make_unique<LayoutHistory>();
    const DockLayout first = with_ratio(panels, 0.3f);
    const DockLayout second = with_ratio(panels, 0.4f);
    const DockLayout third = with_ratio(panels, 0.5f);

    reset(*history, first);
    push(*history, second);
    push(*history, third);
    CHECK(can_undo(*history));
    CHECK_FALSE(can_redo(*history));

    DockLayout out;
    REQUIRE(undo(*history, out));
    CHECK(equal(out, second));
    REQUIRE(undo(*history, out));
    CHECK(equal(out, first));
    CHECK_FALSE(can_undo(*history));
    CHECK_FALSE(undo(*history, out));
    CHECK(equal(out, first));

    REQUIRE(redo(*history, out));
    CHECK(equal(out, second));
    REQUIRE(redo(*history, out));
    CHECK(equal(out, third));
    CHECK_FALSE(redo(*history, out));
}

TEST_CASE("LayoutHistory: a push after undo drops the redo branch")
{
    const PanelTable panels = make_panels();
    auto history = std::make_unique<LayoutHistory>();
    reset(*history, with_ratio(panels, 0.3f));
    push(*history, with_ratio(panels, 0.4f));
    push(*history, with_ratio(panels, 0.5f));

    DockLayout out;
    REQUIRE(undo(*history, out));
    REQUIRE(undo(*history, out));
    push(*history, with_ratio(panels, 0.6f));
    CHECK_FALSE(can_redo(*history));
    REQUIRE(undo(*history, out));
    CHECK(equal(out, with_ratio(panels, 0.3f)));
}

TEST_CASE("LayoutHistory: pushing an equal layout is a no-op")
{
    const PanelTable panels = make_panels();
    auto history = std::make_unique<LayoutHistory>();
    reset(*history, with_ratio(panels, 0.3f));
    push(*history, with_ratio(panels, 0.3f));
    CHECK(history->count == 1);
    CHECK_FALSE(can_undo(*history));
}

TEST_CASE("LayoutHistory: an unset history starts at the first push")
{
    const PanelTable panels = make_panels();
    auto history = std::make_unique<LayoutHistory>();
    CHECK_FALSE(can_undo(*history));
    CHECK_FALSE(can_redo(*history));
    DockLayout out;
    CHECK_FALSE(undo(*history, out));
    push(*history, with_ratio(panels, 0.3f));
    CHECK(history->count == 1);
}

TEST_CASE("LayoutHistory: the ring drops the oldest snapshot when full")
{
    const PanelTable panels = make_panels();
    auto history = std::make_unique<LayoutHistory>();
    const uint32_t total = k_dock_history_size + 8;
    reset(*history, with_ratio(panels, 0.10f));
    for (uint32_t i = 1; i < total; ++i)
        push(*history, with_ratio(panels, 0.10f + 0.01f * static_cast<float>(i)));
    CHECK(history->count == k_dock_history_size);

    DockLayout out;
    uint32_t undone = 0;
    while (undo(*history, out))
        ++undone;
    CHECK(undone == k_dock_history_size - 1);
    CHECK(equal(out, with_ratio(panels, 0.10f + 0.01f * static_cast<float>(total - k_dock_history_size))));

    uint32_t redone = 0;
    while (redo(*history, out))
        ++redone;
    CHECK(redone == k_dock_history_size - 1);
    CHECK(equal(out, with_ratio(panels, 0.10f + 0.01f * static_cast<float>(total - 1))));
}

TEST_CASE("LayoutHistory: drives a real edit sequence")
{
    const PanelTable panels = make_panels();
    auto history = std::make_unique<LayoutHistory>();
    DockLayout layout = make_sample(panels);
    reset(*history, layout);

    require_applied(dock_panel(layout, panels, pid("c"), 2, DropZone::Bottom));
    push(*history, layout);
    require_applied(close_panel(layout, panels, pid("b")));
    push(*history, layout);

    DockLayout out;
    REQUIRE(undo(*history, out));
    CHECK(dump(out, panels).find("closed") == std::string::npos);
    REQUIRE(undo(*history, out));
    CHECK(equal(out, make_sample(panels)));
}
