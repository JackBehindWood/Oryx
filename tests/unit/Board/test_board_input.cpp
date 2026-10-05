#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Core/PolledInput.h"

using namespace oryx;

TEST_CASE("read_board_input maps devices to board intents in one place")
{
    PolledInput input;
    input.set_cursor(12.0f, 34.0f);

    BoardInput idle = read_board_input(input, { 800.0f, 600.0f });
    CHECK(idle.viewport == Vec2f(800.0f, 600.0f));
    CHECK(idle.cursor == Vec2f(12.0f, 34.0f));
    CHECK_FALSE(idle.select);
    CHECK_FALSE(idle.back);
    CHECK_FALSE(idle.undo);
    CHECK_FALSE(idle.restart);
    CHECK_FALSE(idle.quit);

    input.set_mouse_button(MouseCode::Left, true);
    BoardInput click = read_board_input(input, {});
    CHECK(click.select);
    CHECK(click.restart);

    input.begin_frame();
    BoardInput held = read_board_input(input, {});
    CHECK_FALSE(held.select);

    input.set_mouse_button(MouseCode::Right, true);
    input.set_key(KeyCode::U, true);
    input.set_key(KeyCode::R, true);
    input.set_key(KeyCode::Escape, true);
    BoardInput keys = read_board_input(input, {});
    CHECK(keys.back);
    CHECK(keys.undo);
    CHECK(keys.restart);
    CHECK(keys.quit);
    CHECK_FALSE(keys.select);

    input.begin_frame();
    input.set_key(KeyCode::Backspace, true);
    CHECK(read_board_input(input, {}).back);
}
