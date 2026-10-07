#include "doctest.h"

#include "NullWindow.h"
#include "Oryx.h"
#include "unit/Interface/support/ImTestDriver.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct TestContext : ImContext
{
};

const Vec2f k_surface = { 200.0f, 100.0f };

} // namespace

TEST_CASE("make_im_input: the wheel arrives in lines and resets next frame")
{
    NullWindow window({ "Test", 200, 100 });
    window.inject_scroll(0.0f, 1.0f);
    window.inject_scroll(0.5f, 0.25f);
    ImInput input = make_im_input(window.input(), 3, k_surface, 2.0f, 0.016f);
    CHECK(input.wheel[0] == doctest::Approx(0.5f));
    CHECK(input.wheel[1] == doctest::Approx(1.25f));
    CHECK(input.surface == 3);
    CHECK(input.scale == 2.0f);
    CHECK(input.delta_time == doctest::Approx(0.016f));

    window.poll_events();
    input = make_im_input(window.input(), 3, k_surface, 2.0f, 0.016f);
    CHECK(input.wheel[0] == 0.0f);
    CHECK(input.wheel[1] == 0.0f);
}

TEST_CASE("make_im_input: the pointer is valid only inside the surface")
{
    NullWindow window({ "Test", 200, 100 });
    window.inject_cursor(10.0f, 20.0f);
    window.inject_mouse_button(MouseCode::Left, true);
    ImInput input = make_im_input(window.input(), 0, k_surface, 1.0f, 0.0f);
    CHECK(input.pointer.valid);
    CHECK(input.pointer.position[0] == 10.0f);
    CHECK(button_of(input, MouseCode::Left).down);
    CHECK(button_of(input, MouseCode::Left).pressed);

    window.inject_cursor(200.0f, 20.0f);
    CHECK_FALSE(make_im_input(window.input(), 0, k_surface, 1.0f, 0.0f).pointer.valid);
    window.inject_cursor(-1.0f, 20.0f);
    CHECK_FALSE(make_im_input(window.input(), 0, k_surface, 1.0f, 0.0f).pointer.valid);
}

TEST_CASE("make_im_input: navigation keys and modifiers map")
{
    struct Pair
    {
        KeyCode code;
        ImKey key;
    };
    const Pair pairs[] = {
        { KeyCode::Escape, ImKey::Escape }, { KeyCode::Enter, ImKey::Enter }, { KeyCode::Tab, ImKey::Tab },
        { KeyCode::Backspace, ImKey::Backspace }, { KeyCode::Delete, ImKey::Delete }, { KeyCode::Left, ImKey::Left },
        { KeyCode::Right, ImKey::Right }, { KeyCode::Up, ImKey::Up }, { KeyCode::Down, ImKey::Down },
        { KeyCode::Home, ImKey::Home }, { KeyCode::End, ImKey::End }, { KeyCode::PageUp, ImKey::PageUp },
        { KeyCode::PageDown, ImKey::PageDown },
    };
    for (const Pair& pair : pairs)
    {
        NullWindow window({ "Test", 200, 100 });
        window.inject_key(pair.code, true);
        ImInput input = make_im_input(window.input(), 0, k_surface, 1.0f, 0.0f);
        CHECK(input.keys.down == im_key_bit(pair.key));
        CHECK(input.keys.pressed == im_key_bit(pair.key));

        window.poll_events();
        input = make_im_input(window.input(), 0, k_surface, 1.0f, 0.0f);
        CHECK(key_down(input.keys, pair.key));
        CHECK_FALSE(key_pressed(input.keys, pair.key));
    }

    NullWindow window({ "Test", 200, 100 });
    window.inject_key(KeyCode::RightShift, true);
    window.inject_key(KeyCode::LeftSuper, true);
    const ImInput input = make_im_input(window.input(), 0, k_surface, 1.0f, 0.0f);
    CHECK(input.keys.shift);
    CHECK(input.keys.super);
    CHECK_FALSE(input.keys.ctrl);
    CHECK_FALSE(input.keys.alt);
}

TEST_CASE("ImContext: the wheel has a single consumer per frame")
{
    TestContext context;
    ImTestDriver<TestContext> driver(context, k_surface);
    driver.wheel({ 0.0f, 2.0f });
    driver.frame([&] {
        CHECK_FALSE(context.wheel_consumed());
        CHECK(context.consume_wheel()[1] == 2.0f);
        CHECK(context.wheel_consumed());
        CHECK(context.consume_wheel()[1] == 0.0f);
    });
    driver.frame([&] { CHECK(context.consume_wheel()[1] == 0.0f); });
    CHECK_THROWS_AS(std::ignore = context.consume_wheel(), Error);
}
