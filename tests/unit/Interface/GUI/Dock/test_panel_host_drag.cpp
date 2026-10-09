#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/GUI/Dock/DockTestSupport.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::gui;
using namespace oryx::test;

namespace
{

// Nodes of make_sample: 0 split (ratio 0.7), 1 tabs [a b*], 2 tabs [vp*].
struct DragScene
{
    GuiFixture f;
    DockLayout& layout = f.context.dock_model().layout;
    DockLayout original;

    DragScene()
    {
        f.driver.input().surface_size = { 800.0f, 600.0f };
        model().panels = make_panels();
        layout = make_sample(model().panels);
        settle_dock(model());
        original = layout;
        frames(4);
    }

    DockView& panel_host() { return f.context.dock_view(); }
    DockModel& model() { return f.context.dock_model(); }
    PanelTable& table() { return model().panels; }
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
        for (uint32_t step = 1; step <= 4; ++step)
        {
            f.driver.move_to(from + (at - from) * (static_cast<float>(step) / 4.0f));
            frames(1);
        }
    }

    void release_drag()
    {
        f.driver.release();
        frames(4);
    }

    void drag(const Vec2f& from, const Vec2f& to)
    {
        press_at(from);
        move_to(to);
        release_drag();
    }
};

Vec2f centre(const Rect& rect) { return Vec2f(rect.min[0] + rect.size[0] * 0.5f, rect.min[1] + rect.size[1] * 0.5f); }

PanelId tab_at(const DockLayout& layout, int32_t node, uint32_t index) { return layout.nodes[node].tabs[index]; }

} // namespace

TEST_CASE("panel host drag: a tab dropped on the left half of its sibling reorders and one undo restores")
{
    DragScene scene;
    const SolvedNode& strip = scene.solved().nodes[1];
    REQUIRE(tab_at(scene.layout, 1, 1) == pid("b"));
    const Vec2f from = centre(strip.tab_rects[1]);
    const Vec2f to(strip.tab_rects[0].min[0] + 4.0f, strip.tab_rects[0].min[1] + 4.0f);
    scene.drag(from, to);

    CHECK(tab_at(scene.layout, 1, 0) == pid("b"));
    CHECK(tab_at(scene.layout, 1, 1) == pid("a"));
    CHECK(can_undo_layout());

    undo_layout();
    scene.frames(2);
    CHECK(diff(scene.layout, scene.original).empty());
    CHECK_FALSE(can_undo_layout());
    CHECK(can_redo_layout());

    redo_layout();
    scene.frames(2);
    CHECK(tab_at(scene.layout, 1, 0) == pid("b"));
}

TEST_CASE("panel host drag: a reorder in place changes nothing and records no history")
{
    DragScene scene;
    const SolvedNode& strip = scene.solved().nodes[1];
    const Vec2f from = centre(strip.tab_rects[1]);
    scene.drag(from, from + Vec2f(6.0f, 0.0f));
    CHECK(diff(scene.layout, scene.original).empty());
    CHECK_FALSE(can_undo_layout());
}

TEST_CASE("panel host drag: dragging a tab out of the surface tears it off to a float")
{
    DragScene scene;
    const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
    scene.drag(from, Vec2f(900.0f, 300.0f));
    REQUIRE(scene.layout.float_count == 1);
    CHECK(scene.layout.floats[0].panel == pid("b"));
    CHECK(scene.layout.nodes[1].count == 1);

    undo_layout();
    scene.frames(2);
    CHECK(diff(scene.layout, scene.original).empty());
}

TEST_CASE("panel host drag: shift floats a tab over the surface and a panel without tear_off stays put")
{
    {
        DragScene scene;
        scene.f.driver.input().keys.shift = true;
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, centre(scene.solved().nodes[2].body));
        CHECK(scene.layout.float_count == 1);
    }
    {
        DragScene scene;
        set_flags(scene.table(), "b", panel_flag::all & ~panel_flag::tear_off);
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, Vec2f(900.0f, 300.0f));
        CHECK(scene.layout.float_count == 0);
        CHECK(diff(scene.layout, scene.original).empty());
    }
}

TEST_CASE("panel host drag: a float moves over a body, and re-docks on an edge zone")
{
    DragScene scene;
    scene.drag(centre(scene.solved().nodes[1].tab_rects[1]), Vec2f(900.0f, 300.0f));
    REQUIRE(scene.layout.float_count == 1);

    const Rect title = scene.solved().float_parts[0].title;
    const Vec2f target = centre(scene.solved().nodes[1].body);
    scene.drag(centre(title), target);
    REQUIRE(scene.layout.float_count == 1);
    CHECK(scene.layout.floats[0].rect.min[0] < title.min[0]);

    const Rect moved = scene.solved().float_parts[0].title;
    const Rect body = scene.solved().nodes[1].body;
    scene.press_at(centre(moved));
    scene.move_to(Vec2f(body.min[0] + body.size[0] - 10.0f, body.min[1] + body.size[1] * 0.5f));
    CHECK(scene.model().drag.source == DragSource::Float);
    scene.release_drag();
    CHECK(scene.layout.float_count == 0);
    CHECK(scene.layout.node_count == 5);
    CHECK(is_open(scene.layout, pid("b")));
    CHECK_NOTHROW(validate(scene.layout, scene.table(), ValidateFlags{ true }));
}

TEST_CASE("panel host drag: a forbidden target shows no preview, refuses the drop and leaves the layout alone")
{
    DragScene scene;
    REQUIRE(dock_only_in("b", "vp"));
    const Rect body = scene.solved().nodes[1].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(Vec2f(body.min[0] + body.size[0] - 10.0f, body.min[1] + body.size[1] * 0.5f));

    const DropPlan& plan = scene.model().drag.plan;
    CHECK(plan.action == DropAction::Cancel);
    CHECK(plan.reason == DockReason::NotPermittedTarget);
    CHECK(is_empty(plan.preview));
    CHECK(panel_host_result().dragging);

    scene.release_drag();
    CHECK(diff(scene.layout, scene.original).empty());
    CHECK_FALSE(panel_host_result().dragging);
}

TEST_CASE("panel host drag: never_dock_in blocks the node but another node still takes the panel")
{
    DragScene scene;
    REQUIRE(never_dock_in("b", "a"));
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.drag(centre(scene.solved().nodes[1].tab_rects[1]), centre(vp_body));
    CHECK_FALSE(diff(scene.layout, scene.original).empty());
    REQUIRE(is_open(scene.layout, pid("b")));

    DragScene second;
    REQUIRE(never_dock_in("b", "vp"));
    const Rect vp = second.solved().nodes[2].body;
    second.drag(centre(second.solved().nodes[1].tab_rects[1]), centre(vp));
    CHECK(diff(second.layout, second.original).empty());
}

TEST_CASE("panel host drag: escape cancels and the layout is unchanged")
{
    DragScene scene;
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(centre(vp_body));
    REQUIRE(panel_host_result().dragging);
    scene.f.driver.key_press(ImKey::Escape);
    scene.frames(1);
    CHECK_FALSE(panel_host_result().dragging);
    scene.release_drag();
    CHECK(diff(scene.layout, scene.original).empty());
}

TEST_CASE("panel host drag: ctrl+tab cycles the focused node's tabs and wraps, shift reverses")
{
    DragScene scene;
    focus_panel("b");
    auto press_tab = [&](bool shift)
    {
        scene.f.driver.input().keys.ctrl = true;
        scene.f.driver.input().keys.shift = shift;
        scene.f.driver.key_press(ImKey::Tab);
        scene.frames(1);
    };
    REQUIRE(scene.layout.nodes[1].selected == 1);
    press_tab(false);
    CHECK(scene.layout.nodes[1].selected == 0);
    CHECK(is_panel_focused("a"));
    press_tab(true);
    CHECK(scene.layout.nodes[1].selected == 1);
    CHECK(is_panel_focused("b"));
}

TEST_CASE("panel host drag: warm drag frames allocate nothing")
{
    DragScene scene;
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(centre(vp_body));
    scene.move_to(centre(vp_body) + Vec2f(5.0f, 5.0f));
    const MemoryStats before = test::all_allocations();
    scene.move_to(centre(vp_body) + Vec2f(12.0f, 9.0f));
    scene.frames(2);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
    scene.release_drag();
}

TEST_CASE("panel host drag: a panel toolbar is offered only when reserved and the style can hide it")
{
    GuiFixture f;
    f.driver.input().surface_size = { 800.0f, 600.0f };
    DockModel& state = f.context.dock_model();
    state.panels = make_panels();
    find_panel(state.panels, pid("a"))->toolbar = true;
    DockLayout& layout = state.layout;
    layout = make_sample(state.panels);
    layout.nodes[1].selected = 0;
    settle_dock(state);
    bool a_toolbar = false;
    auto build = f.frame_of(
        [&]
        {
            PanelHostScope host;
            PanelScope a("a");
            if (a.visible())
            {
                PanelToolbarScope bar;
                a_toolbar = bar.visible();
                if (bar.visible())
                    gui::label("tools");
            }
        });
    f.driver.run_frames(4, build);
    CHECK(a_toolbar);
    CHECK_FALSE(is_empty(panel_toolbar_rect("a")));

    set_dock_style([](DockStyle& style) { style.toolbars = false; });
    f.driver.run_frames(4, build);
    CHECK_FALSE(a_toolbar);
    CHECK(is_empty(panel_toolbar_rect("a")));
}

namespace
{

const DropGuide* find_guide(const DropGuides& guides, int32_t node, DropZone zone)
{
    for (uint32_t i = 0; i < guides.count; ++i)
        if (guides.guides[i].node == node && guides.guides[i].zone == zone)
            return &guides.guides[i];
    return nullptr;
}

}

TEST_CASE("panel host drag: dropping on a compass guide docks exactly like dock_panel and the hovered guide is reported")
{
    DragScene scene;
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(centre(vp_body));
    const DropGuides guides = drop_guides(scene.layout, scene.table(), scene.solved(), pid("b"), centre(vp_body));
    const DropGuide* top = find_guide(guides, 2, DropZone::Top);
    REQUIRE(top != nullptr);
    scene.move_to(centre(top->rect));
    CHECK(scene.model().drag.plan.guide >= 0);
    CHECK(scene.model().drag.plan.zone == DropZone::Top);

    DockLayout expected = scene.original;
    REQUIRE(dock_panel(expected, scene.table(), pid("b"), 2, DropZone::Top).applied);
    scene.release_drag();
    CHECK(equal(scene.layout, expected));
}

TEST_CASE("panel host drag: guides can be turned off and the bands still decide")
{
    DragScene scene;
    set_dock_style([](DockStyle& style) { style.guides = false; });
    scene.frames(2);
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(centre(vp_body));
    CHECK(scene.model().drag.plan.guide == -1);
    CHECK(scene.model().drag.plan.action == DropAction::Dock);
    scene.release_drag();
}

TEST_CASE("panel host drag: the panel that landed flashes, then the flash ends")
{
    DragScene scene;
    scene.f.driver.input().delta_time = 0.1f;
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(centre(vp_body));
    scene.f.driver.release();
    scene.frames(1);
    CHECK(panel_host_result().landed == pid("b"));
    CHECK(scene.model().landed == pid("b"));
    scene.frames(2);
    CHECK(is_valid(scene.model().landed));
    scene.frames(8);
    CHECK_FALSE(is_valid(scene.model().landed));
}

TEST_CASE("panel host drag: no flash when the style turns it off or the drop changed nothing")
{
    {
        DragScene scene;
        set_dock_style([](DockStyle& style) { style.drop_flash = false; });
        scene.frames(2);
        const Rect vp_body = scene.solved().nodes[2].body;
        scene.drag(centre(scene.solved().nodes[1].tab_rects[1]), centre(vp_body));
        scene.frames(2);
        CHECK_FALSE(is_valid(scene.model().landed));
    }
    {
        DragScene scene;
        const Vec2f from = centre(scene.solved().nodes[1].tab_rects[1]);
        scene.drag(from, from + Vec2f(6.0f, 0.0f));
        CHECK_FALSE(is_valid(scene.model().landed));
    }
}

TEST_CASE("panel host drag: warm frames hovering a compass guide allocate nothing")
{
    DragScene scene;
    const Rect vp_body = scene.solved().nodes[2].body;
    scene.press_at(centre(scene.solved().nodes[1].tab_rects[1]));
    scene.move_to(centre(vp_body));
    const DropGuides guides = drop_guides(scene.layout, scene.table(), scene.solved(), pid("b"), centre(vp_body));
    const DropGuide* left = find_guide(guides, 2, DropZone::Left);
    REQUIRE(left != nullptr);
    const Vec2f target = centre(left->rect);
    scene.move_to(target);
    scene.move_to(target + Vec2f(2.0f, 1.0f));
    const MemoryStats before = test::all_allocations();
    scene.move_to(target + Vec2f(-3.0f, 2.0f));
    scene.frames(2);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
    scene.release_drag();
}

TEST_CASE("panel host drag: a tab reorders live while held, and escape puts it back")
{
    DragScene scene;
    const SolvedNode& strip = scene.solved().nodes[1];
    REQUIRE(tab_at(scene.layout, 1, 1) == pid("b"));
    const Rect first = strip.tab_rects[0];
    scene.press_at(centre(strip.tab_rects[1]));
    scene.move_to(Vec2f(first.min[0] + 4.0f, first.min[1] + first.size[1] * 0.5f));
    REQUIRE(panel_host_result().dragging);
    CHECK(tab_at(scene.layout, 1, 0) == pid("b"));
    CHECK(tab_at(scene.layout, 1, 1) == pid("a"));

    scene.f.driver.key_press(ImKey::Escape);
    scene.frames(1);
    CHECK_FALSE(panel_host_result().dragging);
    CHECK(tab_at(scene.layout, 1, 1) == pid("b"));
    scene.release_drag();
    CHECK(diff(scene.layout, scene.original).empty());
    CHECK_FALSE(can_undo_layout());
}

TEST_CASE("panel host drag: a live reorder is one undo step and a refused drop restores the order")
{
    DragScene scene;
    const SolvedNode& strip = scene.solved().nodes[1];
    const Rect first = strip.tab_rects[0];
    scene.press_at(centre(strip.tab_rects[1]));
    scene.move_to(Vec2f(first.min[0] + 4.0f, first.min[1] + first.size[1] * 0.5f));
    CHECK(tab_at(scene.layout, 1, 0) == pid("b"));
    scene.release_drag();
    CHECK(tab_at(scene.layout, 1, 0) == pid("b"));
    undo_layout();
    scene.frames(2);
    CHECK(diff(scene.layout, scene.original).empty());
    CHECK_FALSE(can_undo_layout());

    set_flags(scene.model().panels, "b", panel_flag::all & ~panel_flag::tear_off & ~panel_flag::dock_elsewhere);
    scene.frames(2);
    const SolvedNode& again = scene.solved().nodes[1];
    scene.press_at(centre(again.tab_rects[1]));
    scene.move_to(Vec2f(again.tab_rects[0].min[0] + 4.0f, again.tab_rects[0].min[1] + 4.0f));
    CHECK(tab_at(scene.layout, 1, 0) == pid("b"));
    scene.move_to(Vec2f(900.0f, 300.0f));
    scene.release_drag();
    CHECK(tab_at(scene.layout, 1, 0) == pid("a"));
}
