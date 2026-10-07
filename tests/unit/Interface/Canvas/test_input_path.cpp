#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Interface/Canvas/ImInputDevice.h"
#include "NullWindow.h"

using namespace oryx;

TEST_CASE("input path: typed text arrives as UTF-8 for one frame")
{
    PolledInput input;
    input.add_text('a');
    input.add_text(0xE9);
    CHECK(input.typed_text() == "a\xC3\xA9");
    input.begin_frame();
    CHECK(input.typed_text().empty());
}

TEST_CASE("input path: a key repeat is a one-frame edge that is not a press")
{
    PolledInput input;
    input.set_key(KeyCode::Backspace, true);
    input.begin_frame();
    input.set_key_repeat(KeyCode::Backspace);
    CHECK(input.key_repeated(KeyCode::Backspace));
    CHECK_FALSE(input.key_pressed(KeyCode::Backspace));
    input.begin_frame();
    CHECK_FALSE(input.key_repeated(KeyCode::Backspace));
}

TEST_CASE("input path: paste text lives for the frame it arrived in")
{
    PolledInput input;
    input.set_paste_text("clip");
    CHECK(input.paste_text() == "clip");
    input.begin_frame();
    CHECK(input.paste_text().empty());
}

TEST_CASE("input path: make_im_input carries text, repeats, paste and the shortcut modifier")
{
    PolledInput input;
    input.add_text('x');
    input.set_key(KeyCode::A, true);
    input.set_key_repeat(KeyCode::Left);
    input.set_paste_text("p");
    input.set_key(KeyCode::LeftSuper, true);
    input.set_key(KeyCode::LeftControl, true);
    const ImInput result = make_im_input(input, 0, { 100.0f, 100.0f }, 1.0f, 0.016f);
    CHECK(result.text == "x");
    CHECK(result.paste == "p");
    CHECK(key_pressed(result.keys, ImKey::A));
    CHECK(key_stroke(result.keys, ImKey::Left));
    CHECK_FALSE(key_pressed(result.keys, ImKey::Left));
    CHECK(result.keys.shortcut);
}

TEST_CASE("input path: the null window injects text, repeats and paste and keeps a clipboard")
{
    NullWindow window(WindowDesc{ "Test", 100, 100 });
    window.inject_text("h\xC3\xA9");
    window.inject_key_repeat(KeyCode::Delete);
    window.inject_paste("pasted");
    CHECK(window.input().typed_text() == "h\xC3\xA9");
    CHECK(window.input().key_repeated(KeyCode::Delete));
    CHECK(window.input().paste_text() == "pasted");
    window.poll_events();
    CHECK(window.input().typed_text().empty());
    window.set_clipboard_text("copied");
    CHECK(window.clipboard_text() == "copied");
}

TEST_CASE("input path: an IInput without text support compiles and reads empty")
{
    class Silent final : public IInput
    {
    public:
        bool key_down(KeyCode) const override { return false; }
        bool key_pressed(KeyCode) const override { return false; }
        bool key_released(KeyCode) const override { return false; }
        bool mouse_down(MouseCode) const override { return false; }
        bool mouse_pressed(MouseCode) const override { return false; }
        bool mouse_released(MouseCode) const override { return false; }
        void cursor_position(Vec2f& out) const override { out = Vec2f(0.0f, 0.0f); }
        void scroll_delta(Vec2f& out) const override { out = Vec2f(0.0f, 0.0f); }
    };
    Silent silent;
    CHECK(silent.typed_text().empty());
    CHECK_FALSE(silent.key_repeated(KeyCode::A));
    CHECK(silent.paste_text().empty());
}
