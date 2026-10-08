#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

constexpr Vec2f k_surface{ 400.0f, 300.0f };
const ViewRegion k_board{ { 0.0f, 20.0f }, { 300.0f, 280.0f } };

struct Rig
{
    PolledInput input;
    InputRouter router;

    // One frame: the device moves first, then the interface phase (`claim`), then the world phase.
    void frame(float x, float y)
    {
        input.begin_frame();
        input.set_cursor(x, y);
    }

    void begin(bool claim_pointer = false, bool claim_keyboard = false, bool set_board = true)
    {
        router.begin_frame(input, k_surface);
        if (set_board)
        {
            router.set_view(k_main_view, k_board);
        }
        if (claim_pointer)
        {
            router.claim_pointer();
        }
        if (claim_keyboard)
        {
            router.claim_keyboard();
        }
        router.begin_world();
    }

    [[nodiscard]] bool world_down() const { return router.world_input(k_main_view).mouse_down(MouseCode::Left); }
    [[nodiscard]] bool world_pressed() const { return router.world_input(k_main_view).mouse_pressed(MouseCode::Left); }
    [[nodiscard]] bool world_released() const { return router.world_input(k_main_view).mouse_released(MouseCode::Left); }
    [[nodiscard]] bool ui_down() const { return router.interface_input().mouse_down(MouseCode::Left); }
    [[nodiscard]] Vec2f world_cursor() const
    {
        Vec2f cursor;
        router.world_input(k_main_view).cursor_position(cursor);
        return cursor;
    }
};

} // namespace

TEST_CASE("InputRouter gives a world view region-local coordinates")
{
    Rig rig;
    rig.frame(50.0f, 70.0f);
    rig.begin();
    CHECK(rig.world_cursor()[0] == doctest::Approx(50.0f));
    CHECK(rig.world_cursor()[1] == doctest::Approx(50.0f));
}

TEST_CASE("InputRouter default main view is the whole surface and filters nothing")
{
    Rig rig;
    rig.frame(120.0f, 90.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.input.set_key(KeyCode::R, true);
    rig.begin(false, false, false);

    CHECK(rig.router.view(k_main_view).size[0] == doctest::Approx(400.0f));
    CHECK(rig.world_pressed());
    CHECK(rig.world_cursor()[0] == doctest::Approx(120.0f));
    CHECK(rig.router.world_input(k_main_view).key_pressed(KeyCode::R));
    CHECK(rig.router.pointer_owner() == PointerOwner::World);
}

TEST_CASE("InputRouter: a press the interface claims never reaches the world")
{
    Rig rig;
    rig.frame(100.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.begin(true);

    CHECK(rig.router.pointer_owner() == PointerOwner::Interface);
    CHECK_FALSE(rig.world_pressed());
    CHECK_FALSE(rig.world_down());
    CHECK(rig.router.interface_input().mouse_pressed(MouseCode::Left));
    CHECK(rig.world_cursor()[0] < -1000.0f);
}

TEST_CASE("InputRouter: a press outside every view belongs to the interface")
{
    Rig rig;
    rig.frame(350.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.begin();

    CHECK(rig.router.pointer_owner() == PointerOwner::Interface);
    CHECK_FALSE(rig.world_pressed());
}

TEST_CASE("InputRouter: a press on the world stays the world's while dragged over the interface")
{
    Rig rig;
    rig.frame(100.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.begin();
    CHECK(rig.world_pressed());

    rig.frame(350.0f, 100.0f);
    rig.begin(true);
    CHECK(rig.router.pointer_owner() == PointerOwner::World);
    CHECK(rig.world_down());
    CHECK(rig.world_cursor()[0] == doctest::Approx(350.0f));
    CHECK_FALSE(rig.ui_down());
    Vec2f hidden;
    rig.router.interface_input().cursor_position(hidden);
    CHECK(hidden[0] < -1000.0f);

    rig.frame(350.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, false);
    rig.begin(true);
    CHECK(rig.world_released());
    CHECK(rig.router.pointer_owner() == PointerOwner::World);

    rig.frame(350.0f, 100.0f);
    rig.begin();
    CHECK(rig.router.pointer_owner() == PointerOwner::None);
}

TEST_CASE("InputRouter: a press on the interface stays the interface's when released over the world")
{
    Rig rig;
    rig.frame(350.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.begin(true);
    CHECK(rig.router.pointer_owner() == PointerOwner::Interface);

    rig.frame(100.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, false);
    rig.begin();
    CHECK(rig.router.pointer_owner() == PointerOwner::Interface);
    CHECK_FALSE(rig.world_released());
    CHECK(rig.router.interface_input().mouse_released(MouseCode::Left));
    CHECK_FALSE(rig.world_pressed());
}

TEST_CASE("InputRouter: hover follows claims and a lost release cannot wedge the latch")
{
    Rig rig;
    rig.frame(100.0f, 100.0f);
    rig.begin();
    CHECK(rig.world_cursor()[0] == doctest::Approx(100.0f));

    rig.frame(100.0f, 100.0f);
    rig.begin(true);
    CHECK(rig.world_cursor()[0] < -1000.0f);

    rig.frame(100.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.begin();
    CHECK(rig.router.pointer_owner() == PointerOwner::World);

    // Focus loss: the button is up with no release edge ever delivered.
    rig.frame(100.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, false);
    rig.input.begin_frame();
    rig.begin();
    CHECK(rig.router.pointer_owner() == PointerOwner::None);
}

TEST_CASE("InputRouter: any button's press latches, and a second button does not steal it")
{
    Rig rig;
    rig.frame(100.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Right, true);
    rig.begin();
    CHECK(rig.router.pointer_owner() == PointerOwner::World);
    CHECK(rig.router.world_input(k_main_view).mouse_pressed(MouseCode::Right));

    rig.frame(350.0f, 100.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.begin(true);
    CHECK(rig.router.pointer_owner() == PointerOwner::World);
    CHECK(rig.router.world_input(k_main_view).mouse_pressed(MouseCode::Left));
}

TEST_CASE("InputRouter: wheel goes to whoever the pointer is over")
{
    Rig rig;
    rig.frame(100.0f, 100.0f);
    rig.input.add_scroll(0.0f, 2.0f);
    rig.begin();
    Vec2f wheel;
    rig.router.world_input(k_main_view).scroll_delta(wheel);
    CHECK(wheel[1] == doctest::Approx(2.0f));

    rig.frame(100.0f, 100.0f);
    rig.input.add_scroll(0.0f, 2.0f);
    rig.begin(true);
    rig.router.world_input(k_main_view).scroll_delta(wheel);
    CHECK(wheel[1] == doctest::Approx(0.0f));
    rig.router.interface_input().scroll_delta(wheel);
    CHECK(wheel[1] == doctest::Approx(2.0f));
}

TEST_CASE("InputRouter: a keyboard claim hides every key, text and quit from the world")
{
    Rig rig;
    rig.frame(100.0f, 100.0f);
    rig.input.set_key(KeyCode::Escape, true);
    rig.input.add_text('a');
    rig.begin(false, true);

    CHECK_FALSE(rig.router.world_input(k_main_view).key_pressed(KeyCode::Escape));
    CHECK_FALSE(rig.router.world_input(k_main_view).key_down(KeyCode::Escape));
    CHECK(rig.router.world_input(k_main_view).typed_text().empty());
    CHECK(rig.router.interface_input().key_pressed(KeyCode::Escape));
    CHECK(rig.router.interface_input().typed_text() == "a");

    rig.frame(100.0f, 100.0f);
    rig.input.set_key(KeyCode::Escape, false);
    rig.input.set_key(KeyCode::U, true);
    rig.begin();
    CHECK(rig.router.world_input(k_main_view).key_pressed(KeyCode::U));
}

TEST_CASE("InputRouter: two views each see their own region-local cursor")
{
    Rig rig;
    const ViewRegion second{ { 300.0f, 0.0f }, { 100.0f, 300.0f } };
    rig.frame(310.0f, 40.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.router.begin_frame(rig.input, k_surface);
    rig.router.set_view(k_main_view, k_board);
    rig.router.set_view(1, second);
    rig.router.begin_world();

    CHECK(rig.router.pointer_owner() == PointerOwner::World);
    CHECK(rig.router.owner_view() == 1);
    Vec2f local;
    rig.router.world_input(1).cursor_position(local);
    CHECK(local[0] == doctest::Approx(10.0f));
    CHECK(local[1] == doctest::Approx(40.0f));
    CHECK(rig.router.world_input(1).mouse_pressed(MouseCode::Left));
    CHECK_FALSE(rig.router.world_input(k_main_view).mouse_pressed(MouseCode::Left));
    rig.router.world_input(k_main_view).cursor_position(local);
    CHECK(local[0] < -1000.0f);
}

TEST_CASE("InputRouter rejects view ids past the table and treats an unset view as absent")
{
    Rig rig;
    rig.frame(10.0f, 10.0f);
    rig.begin();
    CHECK_THROWS_AS(rig.router.set_view(k_max_views, k_board), Error);
    CHECK_THROWS_AS((void)rig.router.world_input(k_max_views), Error);
    CHECK_FALSE(rig.router.has_view(3));
    Vec2f local;
    rig.router.world_input(3).cursor_position(local);
    CHECK(local[0] < -1000.0f);
}

TEST_CASE("InputRouter before its first frame reports no input")
{
    InputRouter router;
    Vec2f cursor;
    router.world_input(k_main_view).cursor_position(cursor);
    CHECK(cursor[0] < -1000.0f);
    CHECK_FALSE(router.world_input(k_main_view).mouse_down(MouseCode::Left));
    CHECK_FALSE(router.interface_input().key_down(KeyCode::A));
    CHECK(router.interface_input().typed_text().empty());
}

TEST_CASE("InputRouter: a redefined region moves the cursor origin and a dropped view ends with the release")
{
    Rig rig;
    rig.frame(310.0f, 40.0f);
    rig.input.set_mouse_button(MouseCode::Left, true);
    rig.router.begin_frame(rig.input, k_surface);
    rig.router.set_view(k_main_view, k_board);
    rig.router.set_view(1, { { 300.0f, 0.0f }, { 100.0f, 300.0f } });
    rig.router.begin_world();
    CHECK(rig.router.owner_view() == 1);

    rig.frame(310.0f, 40.0f);
    rig.router.begin_frame(rig.input, k_surface);
    rig.router.set_view(k_main_view, k_board);
    rig.router.set_view(1, { { 200.0f, 0.0f }, { 200.0f, 300.0f } });
    rig.router.begin_world();
    Vec2f local;
    rig.router.world_input(1).cursor_position(local);
    CHECK(local[0] == doctest::Approx(110.0f));

    rig.frame(310.0f, 40.0f);
    rig.input.set_mouse_button(MouseCode::Left, false);
    rig.router.begin_frame(rig.input, k_surface);
    rig.router.set_view(k_main_view, k_board);
    rig.router.begin_world();
    CHECK(rig.router.world_input(1).mouse_released(MouseCode::Left));
    rig.frame(310.0f, 40.0f);
    rig.router.begin_frame(rig.input, k_surface);
    rig.router.begin_world();
    CHECK(rig.router.pointer_owner() == PointerOwner::None);
}
