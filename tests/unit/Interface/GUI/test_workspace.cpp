#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

struct WorkspaceScene
{
    GuiFixture f;
    bool menu = true;
    bool left = false;
    bool right = true;
    float left_width = 30.0f;
    float right_width = 40.0f;
    uint32_t menu_clicks = 0;

    WorkspaceScene() { f.driver.input().surface_size = { 200.0f, 100.0f }; }

    void build()
    {
        gui::begin_workspace("shell");
        if (menu)
        {
            gui::begin_menu_bar("menu");
            if (gui::begin_menu("File"))
            {
                gui::end_menu();
            }
            gui::end_menu_bar();
        }
        gui::begin_body();
        if (left)
        {
            gui::begin_side_panel(gui::Side::Left, left_width);
            gui::label("left");
            gui::end_side_panel();
        }
        gui::central_area();
        if (right)
        {
            gui::begin_side_panel(gui::Side::Right, right_width);
            gui::label("right");
            gui::end_side_panel();
        }
        gui::end_body();
        gui::end_workspace();
    }

    WorkspaceRects solve()
    {
        f.driver.run_frames(3, [this] { build(); });
        return f.context.workspace();
    }
};

} // namespace

TEST_CASE("GUI workspace: without a workspace the central area is the whole surface")
{
    GuiFixture f;
    f.driver.frame([] {});
    const WorkspaceRects rects = f.context.workspace();
    CHECK(rects.central.min == Vec2f(0.0f, 0.0f));
    CHECK(rects.central.size == Vec2f(200.0f, 100.0f));
    CHECK(is_empty(rects.left));
    CHECK(is_empty(rects.right));
}

TEST_CASE("GUI workspace: the right panel and the menu bar leave the rest to the central area")
{
    WorkspaceScene s;
    const WorkspaceRects rects = s.solve();
    CHECK(rects.right.size[0] == doctest::Approx(40.0f));
    CHECK(rects.right.min[0] == doctest::Approx(160.0f));
    CHECK(rects.central.size[0] == doctest::Approx(160.0f));
    CHECK(rects.central.min[0] == doctest::Approx(0.0f));
    CHECK(rects.central.min[1] > 0.0f);
    CHECK(rects.central.min[1] + rects.central.size[1] == doctest::Approx(100.0f));
    CHECK(rects.right.min[1] == doctest::Approx(rects.central.min[1]));
    CHECK(rects.right.size[1] == doctest::Approx(rects.central.size[1]));
}

TEST_CASE("GUI workspace: panels on both sides surround the central area in declaration order")
{
    WorkspaceScene s;
    s.left = true;
    const WorkspaceRects rects = s.solve();
    CHECK(rects.left.min[0] == doctest::Approx(0.0f));
    CHECK(rects.left.size[0] == doctest::Approx(30.0f));
    CHECK(rects.central.min[0] == doctest::Approx(30.0f));
    CHECK(rects.central.size[0] == doctest::Approx(130.0f));
    CHECK(rects.right.min[0] == doctest::Approx(160.0f));
}

TEST_CASE("GUI workspace: no menu bar gives the body the whole height, and no panel the whole width")
{
    WorkspaceScene s;
    s.menu = false;
    s.right = false;
    const WorkspaceRects rects = s.solve();
    CHECK(rects.central.min == Vec2f(0.0f, 0.0f));
    CHECK(rects.central.size == Vec2f(200.0f, 100.0f));
    CHECK(is_empty(rects.right));
}

TEST_CASE("GUI workspace: a panel is at most half the surface")
{
    WorkspaceScene s;
    s.right_width = 500.0f;
    const WorkspaceRects rects = s.solve();
    CHECK(rects.right.size[0] == doctest::Approx(100.0f));
    CHECK(rects.central.size[0] == doctest::Approx(100.0f));
}

TEST_CASE("GUI workspace: a resized surface changes the next solve")
{
    WorkspaceScene s;
    s.solve();
    s.f.driver.input().surface_size = { 300.0f, 150.0f };
    const WorkspaceRects rects = s.solve();
    CHECK(rects.right.min[0] == doctest::Approx(260.0f));
    CHECK(rects.central.size[0] == doctest::Approx(260.0f));
    CHECK(rects.central.min[1] + rects.central.size[1] == doctest::Approx(150.0f));
}

TEST_CASE("GUI workspace: scopes build the same layout as the call forms")
{
    WorkspaceScene call;
    call.solve();
    GuiFixture f;
    f.driver.run_frames(3, [&]
    {
        gui::WorkspaceScope workspace("shell");
        {
            gui::MenuBarScope bar("menu");
            gui::MenuScope file("File");
        }
        gui::BodyScope body;
        gui::central_area();
        gui::SidePanelScope panel(gui::Side::Right, 40.0f);
        gui::label("right");
    });
    CHECK(f.context.workspace().central == call.f.context.workspace().central);
    CHECK(f.context.workspace().right == call.f.context.workspace().right);
}

TEST_CASE("GUI workspace: a warm frame allocates nothing")
{
    WorkspaceScene s;
    s.solve();
    s.f.driver.run_frames(3, [&] { s.build(); });
    const MemoryStats before = test::all_allocations();
    s.f.driver.frame([&] { s.build(); });
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
