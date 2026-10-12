#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

struct AreaScene
{
    GuiFixture f;
    bool top = true;
    bool left = false;
    bool right = true;
    bool bottom = false;
    float top_size = 20.0f;
    float left_size = 30.0f;
    float right_size = 40.0f;
    float bottom_size = 16.0f;
    bool drawer_open = false;
    bool drawer_resizable = false;
    bool fill_scrolls = false;
    Rect fill_rect;
    Rect remaining;

    AreaScene() { f.driver.input().surface_size = { 200.0f, 100.0f }; }

    static gui::AreaOptions options(gui::AreaAnchor anchor, float size)
    {
        gui::AreaOptions area;
        area.anchor = anchor;
        area.size = fixed(size);
        return area;
    }

    void build()
    {
        if (top)
        {
            if (gui::AreaScope bar("top", options(gui::AreaAnchor::Top, top_size)); bar)
            {
                gui::label("menu");
            }
        }
        if (bottom)
        {
            if (gui::AreaScope bar("bottom", options(gui::AreaAnchor::Bottom, bottom_size)); bar)
            {
                gui::label("status");
            }
        }
        if (left)
        {
            if (gui::AreaScope rail("left", options(gui::AreaAnchor::Left, left_size)); rail)
            {
                gui::label("left");
            }
        }
        if (right)
        {
            if (gui::AreaScope rail("right", options(gui::AreaAnchor::Right, right_size)); rail)
            {
                gui::label("right");
            }
        }
        gui::AreaOptions fill;
        fill.scroll = fill_scrolls;
        if (gui::AreaScope body("fill", fill); body)
        {
            gui::label("content");
        }
        gui::AreaOptions drawer = options(gui::AreaAnchor::Bottom, 30.0f);
        drawer.mode = gui::AreaMode::Overlay;
        drawer.open = &drawer_open;
        drawer.resizable = drawer_resizable;
        if (gui::AreaScope panel("drawer", drawer); panel)
        {
            gui::label("console");
        }
        remaining = gui::remaining_area();
        fill_rect = gui::area_rect("fill");
    }

    void solve() { f.driver.run_frames(3, [this] { build(); }); }
};

} // namespace

TEST_CASE("GUI area: reserved edges carve the surface outside-in and fill takes the rest")
{
    AreaScene s;
    s.solve();
    CHECK(gui::area_rect("top") == Rect{ { 0.0f, 0.0f }, { 200.0f, 20.0f } });
    CHECK(gui::area_rect("right") == Rect{ { 160.0f, 20.0f }, { 40.0f, 80.0f } });
    CHECK(s.fill_rect == Rect{ { 0.0f, 20.0f }, { 160.0f, 80.0f } });
}

TEST_CASE("GUI area: declaration order decides which edge owns the corner")
{
    AreaScene s;
    s.left = true;
    s.bottom = true;
    s.solve();
    CHECK(gui::area_rect("bottom") == Rect{ { 0.0f, 84.0f }, { 200.0f, 16.0f } });
    CHECK(gui::area_rect("left") == Rect{ { 0.0f, 20.0f }, { 30.0f, 64.0f } });
    CHECK(gui::area_rect("right") == Rect{ { 160.0f, 20.0f }, { 40.0f, 64.0f } });
    CHECK(s.fill_rect == Rect{ { 30.0f, 20.0f }, { 130.0f, 64.0f } });
}

TEST_CASE("GUI area: with no areas fill is the whole surface")
{
    AreaScene s;
    s.top = false;
    s.right = false;
    s.solve();
    CHECK(s.fill_rect == Rect{ { 0.0f, 0.0f }, { 200.0f, 100.0f } });
}

TEST_CASE("GUI area: a size is clamped to the space left")
{
    AreaScene s;
    s.right_size = 500.0f;
    s.solve();
    CHECK(gui::area_rect("right").size[0] == doctest::Approx(200.0f));
    CHECK(s.fill_rect.size[0] == doctest::Approx(0.0f));
}

TEST_CASE("GUI area: a percent size is a share of the space left and a resized surface follows")
{
    GuiFixture f;
    f.driver.input().surface_size = { 200.0f, 100.0f };
    const auto build = [&]
    {
        gui::AreaOptions area;
        area.anchor = gui::AreaAnchor::Left;
        area.size = percent(0.25f);
        if (gui::AreaScope rail("rail", area); rail)
        {
            gui::label("x");
        }
    };
    f.driver.run_frames(3, build);
    CHECK(gui::area_rect("rail").size[0] == doctest::Approx(50.0f));
    f.driver.input().surface_size = { 400.0f, 100.0f };
    f.driver.run_frames(3, build);
    CHECK(gui::area_rect("rail").size[0] == doctest::Approx(100.0f));
}

TEST_CASE("GUI area: a fit size takes the height the content measured")
{
    GuiFixture f;
    f.driver.input().surface_size = { 200.0f, 100.0f };
    const auto build = [&]
    {
        gui::AreaOptions area;
        area.anchor = gui::AreaAnchor::Top;
        if (gui::AreaScope bar("bar", area); bar)
        {
            gui::label("one");
        }
    };
    f.driver.run_frames(4, build);
    CHECK(gui::area_rect("bar").size[1] > 0.0f);
    CHECK(gui::area_rect("bar").size[1] < 50.0f);
}

TEST_CASE("GUI area: an overlay floats over the rest and reserves nothing")
{
    AreaScene s;
    s.drawer_open = true;
    s.solve();
    CHECK(gui::area_rect("drawer") == Rect{ { 0.0f, 70.0f }, { 160.0f, 30.0f } });
    CHECK(s.fill_rect == Rect{ { 0.0f, 20.0f }, { 160.0f, 80.0f } });
}

TEST_CASE("GUI area: a closed area is skipped, reserves nothing and does not call its body")
{
    AreaScene s;
    s.solve();
    CHECK(is_empty(gui::area_rect("drawer")));
    bool ran = false;
    s.f.driver.frame([&]
    {
        bool open = false;
        gui::AreaOptions area;
        area.open = &open;
        if (gui::AreaScope scoped("hidden", area); scoped)
        {
            ran = true;
        }
        CHECK(gui::remaining_area() == Rect{ { 0.0f, 0.0f }, { 200.0f, 100.0f } });
    });
    CHECK_FALSE(ran);
}

TEST_CASE("GUI area: Escape closes an overlay under the pointer")
{
    AreaScene s;
    s.drawer_open = true;
    s.solve();
    s.f.driver.move_to({ 20.0f, 85.0f });
    s.solve();
    s.f.driver.key_press(ImKey::Escape);
    s.f.driver.frame([&] { s.build(); });
    CHECK_FALSE(s.drawer_open);
}

TEST_CASE("GUI area: dragging the grip resizes a resizable edge")
{
    AreaScene s;
    s.drawer_open = true;
    s.drawer_resizable = true;
    s.solve();
    const Rect before = gui::area_rect("drawer");
    s.f.driver.move_to({ 20.0f, before.min[1] + 1.0f });
    s.solve();
    s.f.driver.press();
    s.f.driver.frame([&] { s.build(); });
    s.f.driver.move_to({ 20.0f, before.min[1] - 19.0f });
    s.f.driver.frame([&] { s.build(); });
    s.f.driver.frame([&] { s.build(); });
    s.f.driver.release();
    s.solve();
    CHECK(gui::area_rect("drawer").size[1] > before.size[1] + 10.0f);
}

TEST_CASE("GUI area: a scrolling area clips its content in a scroll region")
{
    AreaScene s;
    s.fill_scrolls = true;
    s.solve();
    CHECK(s.f.find_text("content") != nullptr);
    CHECK(s.fill_rect == Rect{ { 0.0f, 20.0f }, { 160.0f, 80.0f } });
}

TEST_CASE("GUI area: end_area without an open area throws")
{
    GuiFixture f;
    f.driver.frame([] { CHECK_THROWS_AS(gui::end_area(), Error); });
}

TEST_CASE("GUI area: a warm frame allocates nothing")
{
    AreaScene s;
    s.drawer_open = true;
    s.solve();
    s.f.driver.run_frames(3, [&] { s.build(); });
    const MemoryStats before = test::all_allocations();
    s.f.driver.frame([&] { s.build(); });
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
