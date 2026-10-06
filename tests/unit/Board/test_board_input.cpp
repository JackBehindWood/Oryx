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
    input.set_mouse_button(MouseCode::Left, false);
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

TEST_CASE("read_board_input reports the left button held and released, and Enter as confirm")
{
    PolledInput input;
    input.set_mouse_button(MouseCode::Left, true);
    BoardInput press = read_board_input(input, {});
    CHECK(press.select);
    CHECK(press.select_down);
    CHECK_FALSE(press.select_released);
    CHECK_FALSE(press.confirm);

    input.begin_frame();
    BoardInput held = read_board_input(input, {});
    CHECK_FALSE(held.select);
    CHECK(held.select_down);

    input.set_mouse_button(MouseCode::Left, false);
    BoardInput release = read_board_input(input, {});
    CHECK_FALSE(release.select_down);
    CHECK(release.select_released);

    input.begin_frame();
    input.set_key(KeyCode::Enter, true);
    CHECK(read_board_input(input, {}).confirm);
}

TEST_CASE("Escape quits with no button held and takes back a pick while the left button is held")
{
    PolledInput input;
    input.set_key(KeyCode::Escape, true);
    BoardInput idle = read_board_input(input, {});
    CHECK(idle.quit);
    CHECK_FALSE(idle.back);

    PolledInput dragging;
    dragging.set_mouse_button(MouseCode::Left, true);
    dragging.begin_frame();
    dragging.set_key(KeyCode::Escape, true);
    BoardInput drag = read_board_input(dragging, {});
    CHECK(drag.back);
    CHECK_FALSE(drag.quit);
}

TEST_CASE("The restart hint names exactly the inputs that set restart")
{
    std::string hint = k_restart_hint;
    CHECK(hint.find("click") != std::string::npos);
    CHECK(hint.find("R") != std::string::npos);

    PolledInput click;
    click.set_mouse_button(MouseCode::Left, true);
    CHECK(read_board_input(click, {}).restart);

    PolledInput key;
    key.set_key(KeyCode::R, true);
    CHECK(read_board_input(key, {}).restart);

    PolledInput other;
    other.set_key(KeyCode::Enter, true);
    other.set_mouse_button(MouseCode::Right, true);
    CHECK_FALSE(read_board_input(other, {}).restart);
}
