#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("PolledInput reports level state and one-frame edges")
{
    PolledInput input;

    input.set_key(KeyCode::A, true);
    CHECK(input.key_down(KeyCode::A));
    CHECK(input.key_pressed(KeyCode::A));
    CHECK_FALSE(input.key_released(KeyCode::A));

    input.begin_frame();
    CHECK(input.key_down(KeyCode::A));
    CHECK_FALSE(input.key_pressed(KeyCode::A));

    input.set_key(KeyCode::A, false);
    CHECK_FALSE(input.key_down(KeyCode::A));
    CHECK(input.key_released(KeyCode::A));

    input.begin_frame();
    CHECK_FALSE(input.key_released(KeyCode::A));
}

TEST_CASE("PolledInput ignores repeated state and out-of-range codes")
{
    PolledInput input;
    input.set_key(KeyCode::Space, true);
    input.begin_frame();
    input.set_key(KeyCode::Space, true);
    CHECK_FALSE(input.key_pressed(KeyCode::Space));

    input.set_key(KeyCode::Unknown, true);
    input.set_key(static_cast<KeyCode>(100000), true);
    CHECK_FALSE(input.key_down(KeyCode::Unknown));
    CHECK_FALSE(input.key_down(static_cast<KeyCode>(100000)));
    input.set_mouse_button(static_cast<MouseCode>(99), true);
    CHECK_FALSE(input.mouse_down(static_cast<MouseCode>(99)));
}

TEST_CASE("PolledInput tracks mouse buttons, cursor and scroll")
{
    PolledInput input;
    input.set_mouse_button(MouseCode::Left, true);
    CHECK(input.mouse_down(MouseCode::Left));
    CHECK(input.mouse_pressed(MouseCode::Left));
    CHECK_FALSE(input.mouse_down(MouseCode::Right));

    input.set_cursor(12.5f, 40.0f);
    input.add_scroll(1.0f, 2.0f);
    input.add_scroll(0.5f, 1.0f);
    Vec2f cursor;
    Vec2f scroll;
    input.cursor_position(cursor);
    input.scroll_delta(scroll);
    CHECK(cursor[0] == 12.5f);
    CHECK(cursor[1] == 40.0f);
    CHECK(scroll[0] == 1.5f);
    CHECK(scroll[1] == 3.0f);

    input.begin_frame();
    input.scroll_delta(scroll);
    input.cursor_position(cursor);
    CHECK(scroll[0] == 0.0f);
    CHECK(cursor[0] == 12.5f);
    CHECK(input.mouse_down(MouseCode::Left));
    CHECK_FALSE(input.mouse_pressed(MouseCode::Left));

    input.set_mouse_button(MouseCode::Left, false);
    CHECK(input.mouse_released(MouseCode::Left));
}
