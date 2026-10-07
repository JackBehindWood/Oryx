#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Interface/GUI/GuiTextField.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

TextEditInput typed(std::string_view text)
{
    TextEditInput input;
    input.typed = text;
    return input;
}

TextEditInput stroke(ImKey key, bool shift = false, bool shortcut = false)
{
    TextEditInput input;
    input.keys.pressed = im_key_bit(key);
    input.keys.shift = shift;
    input.keys.shortcut = shortcut;
    return input;
}

} // namespace

TEST_CASE("text edit: typing inserts at the caret and moves it")
{
    char buffer[16] = "ac";
    TextEditState state;
    state.caret = 1;
    state.anchor = 1;
    const TextEditResult result = text_edit_apply(buffer, sizeof(buffer), state, typed("b"));
    CHECK(result.changed);
    CHECK(std::string_view(buffer) == "abc");
    CHECK(state.caret == 2);
}

TEST_CASE("text edit: backspace and delete remove whole code points")
{
    char buffer[16] = "a\xC3\xA9z";
    TextEditState state;
    state.caret = 3;
    state.anchor = 3;
    CHECK(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Backspace)).changed);
    CHECK(std::string_view(buffer) == "az");
    CHECK(state.caret == 1);
    CHECK(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Delete)).changed);
    CHECK(std::string_view(buffer) == "a");
    CHECK_FALSE(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Delete)).changed);
}

TEST_CASE("text edit: arrows move over code points and shift extends the selection")
{
    char buffer[16] = "a\xC3\xA9z";
    TextEditState state;
    state.caret = 0;
    state.anchor = 0;
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Right));
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Right));
    CHECK(state.caret == 3);
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Left, true));
    CHECK(state.caret == 1);
    CHECK(state.anchor == 3);
    CHECK(selection_text(buffer, state) == "\xC3\xA9");
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Home));
    CHECK(state.caret == 0);
    CHECK(state.anchor == 0);
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::End));
    CHECK(state.caret == 4);
}

TEST_CASE("text edit: typing over a selection replaces it")
{
    char buffer[16] = "hello";
    TextEditState state;
    state.anchor = 1;
    state.caret = 4;
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, typed("i"));
    CHECK(std::string_view(buffer) == "hio");
}

TEST_CASE("text edit: text that does not fit is cut on a code point boundary")
{
    char buffer[5] = "ab";
    TextEditState state;
    state.caret = 2;
    state.anchor = 2;
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, typed("\xC3\xA9\xC3\xA9"));
    CHECK(std::string_view(buffer) == "ab\xC3\xA9");
    CHECK(state.caret == 4);
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, typed("zzz"));
    CHECK(std::string_view(buffer) == "ab\xC3\xA9");
}

TEST_CASE("text edit: shortcut A selects all, C reports a copy, X cuts, V pastes one line")
{
    char buffer[32] = "one two";
    TextEditState state;
    CHECK(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::A, false, true)).moved);
    CHECK(selection_text(buffer, state) == "one two");
    CHECK(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::C, false, true)).copy);
    CHECK(std::string_view(buffer) == "one two");
    TextEditResult cut = text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::X, false, true));
    CHECK(cut.copy);
    CHECK(cut.changed);
    CHECK(std::string_view(buffer).empty());
    TextEditInput paste = stroke(ImKey::V, false, true);
    paste.paste = "first\nsecond";
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, paste);
    CHECK(std::string_view(buffer) == "first");
}

TEST_CASE("text edit: Enter submits, Escape cancels and a click places the caret")
{
    char buffer[16] = "abc";
    TextEditState state;
    CHECK(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Enter)).submitted);
    CHECK(text_edit_apply(buffer, sizeof(buffer), state, stroke(ImKey::Escape)).cancelled);
    TextEditInput click;
    click.click = 2;
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, click);
    CHECK(state.caret == 2);
    CHECK(state.anchor == 2);
}

TEST_CASE("text edit: control characters are dropped and a stale caret is clamped")
{
    char buffer[16] = "ab";
    TextEditState state;
    state.caret = 99;
    state.anchor = 99;
    std::ignore = text_edit_apply(buffer, sizeof(buffer), state, typed("\t\x01x"));
    CHECK(std::string_view(buffer) == "abx");
}

TEST_CASE("number parsing: whole strings only, with spaces and a sign allowed")
{
    double value = 0.0;
    CHECK(gui::detail::parse_number(" -2.5 ", value));
    CHECK(value == doctest::Approx(-2.5));
    CHECK(gui::detail::parse_number("+3", value));
    CHECK(value == doctest::Approx(3.0));
    CHECK_FALSE(gui::detail::parse_number("1.2.3", value));
    CHECK_FALSE(gui::detail::parse_number("12abc", value));
    CHECK_FALSE(gui::detail::parse_number("", value));
    CHECK_FALSE(gui::detail::parse_number("nan", value));
    CHECK_FALSE(gui::detail::parse_number("1,5", value));
}

TEST_CASE("text input: a click focuses, typing edits the buffer, Enter submits and releases focus")
{
    GuiFixture f;
    char text[16] = "ab";
    gui::TextInputResult result;
    const auto body = [&] { result = gui::text_input("name", text, sizeof(text)); };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("name"), field));
    f.driver.click({ field.min[0] + field.size[0] - 4.0f, rect_centre(field)[1] }, f.column_of(body));
    CHECK(result.focused);
    f.driver.type("c");
    f.driver.frame(f.column_of(body));
    CHECK(std::string_view(text) == "abc");
    CHECK(result.changed);
    f.driver.key_press(ImKey::Backspace);
    f.driver.frame(f.column_of(body));
    CHECK(std::string_view(text) == "ab");
    f.driver.key_press(ImKey::Enter);
    f.driver.frame(f.column_of(body));
    CHECK(result.submitted);
    CHECK_FALSE(result.focused);
    f.driver.type("zzz");
    f.driver.frame(f.column_of(body));
    CHECK(std::string_view(text) == "ab");
}

TEST_CASE("text input: a press elsewhere releases focus")
{
    GuiFixture f;
    char text[16] = "ab";
    bool flag = false;
    gui::TextInputResult result;
    const auto body = [&]
    {
        result = gui::text_input("name", text, sizeof(text));
        std::ignore = gui::checkbox("other", flag);
    };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("name"), field));
    f.driver.click(rect_centre(field), f.column_of(body));
    CHECK(result.focused);
    f.driver.click(rect_centre(f.find_text("other")->rect), f.column_of(body));
    CHECK_FALSE(result.focused);
    CHECK(flag);
}

TEST_CASE("text input: focus shows the accent border, a caret and a selection")
{
    GuiFixture f;
    char text[16] = "abcd";
    const auto body = [&] { std::ignore = gui::text_input("name", text, sizeof(text)); };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("name"), field));
    const uint32_t rest_boxes = f.context.layout().node_count();
    f.driver.double_click(rect_centre(field), f.column_of(body));
    f.driver.frame(f.column_of(body));
    CHECK(f.context.layout().node_count() > rest_boxes);
    bool ring = false;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const BoxPaint& paint = f.context.layout().node(index).paint;
        ring = ring || (paint.border_width > 0.0f && paint.border.r == f.theme.field.accent.r && paint.border.g == f.theme.field.accent.g);
    }
    CHECK(ring);
    CHECK(f.context.output().cursor == CursorShape::Text);
}

TEST_CASE("text input: copy asks the owner for the selection and paste inserts the clipboard")
{
    GuiFixture f;
    char text[32] = "hello";
    const auto body = [&] { std::ignore = gui::text_input("name", text, sizeof(text)); };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("name"), field));
    f.driver.double_click(rect_centre(field), f.column_of(body));
    f.driver.chord(ImKey::C);
    f.driver.frame(f.column_of(body));
    CHECK(f.context.output().copy_text == "hello");
    f.driver.chord(ImKey::V);
    f.driver.paste("world");
    f.driver.frame(f.column_of(body));
    CHECK(std::string_view(text) == "world");
}

TEST_CASE("input_float: typed text is clamped on Enter and Escape restores the value")
{
    GuiFixture f;
    float value = 1.5f;
    bool changed = false;
    const auto body = [&] { changed = gui::input_float("amount", value, { .min = 0.0f, .max = 10.0f }); };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("amount"), field));
    const Vec2f at = { field.min[0] + field.size[0] - 4.0f, rect_centre(field)[1] };
    f.driver.click(at, f.column_of(body));
    f.driver.frame(f.column_of(body));
    f.driver.chord(ImKey::A);
    f.driver.frame(f.column_of(body));
    f.driver.type("42");
    f.driver.frame(f.column_of(body));
    f.driver.key_press(ImKey::Enter);
    f.driver.frame(f.column_of(body));
    CHECK(changed);
    CHECK(value == doctest::Approx(10.0f));

    f.driver.click(at, f.column_of(body));
    f.driver.frame(f.column_of(body));
    f.driver.chord(ImKey::A);
    f.driver.frame(f.column_of(body));
    f.driver.type("3");
    f.driver.frame(f.column_of(body));
    f.driver.key_press(ImKey::Escape);
    f.driver.frame(f.column_of(body));
    CHECK(value == doctest::Approx(10.0f));
}

TEST_CASE("input_int: a bad number leaves the value alone")
{
    GuiFixture f;
    int32_t value = 7;
    const auto body = [&] { std::ignore = gui::input_int("count", value); };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("count"), field));
    f.driver.click(rect_centre(field), f.column_of(body));
    f.driver.frame(f.column_of(body));
    f.driver.chord(ImKey::A);
    f.driver.frame(f.column_of(body));
    f.driver.type("abc");
    f.driver.frame(f.column_of(body));
    f.driver.key_press(ImKey::Enter);
    f.driver.frame(f.column_of(body));
    CHECK(value == 7);
}

TEST_CASE("slider: a double click turns the bar into a number field that Enter commits")
{
    GuiFixture f;
    float value = 0.25f;
    const auto body = [&] { std::ignore = gui::slider_float("level", value, 0.0f, 1.0f); };
    f.driver.settle(f.column_of(body));
    Rect track;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        const LayoutNode& node = f.context.layout().node(index);
        if (node.paint.has_fill && node.paint.border_width > 0.0f && node.rect.size[0] > 40.0f)
        {
            track = node.rect;
            break;
        }
    }
    REQUIRE(track.size[0] > 40.0f);
    f.driver.double_click(rect_centre(track), f.column_of(body));
    f.driver.frame(f.column_of(body));
    f.driver.type("0.75");
    f.driver.frame(f.column_of(body));
    f.driver.key_press(ImKey::Enter);
    f.driver.frame(f.column_of(body));
    f.driver.frame(f.column_of(body));
    CHECK(value == doctest::Approx(0.75f));
}

TEST_CASE("text input: warm frames allocate nothing while typing")
{
    GuiFixture f;
    char text[64] = "abc";
    const auto body = [&] { std::ignore = gui::text_input("name", text, sizeof(text)); };
    f.driver.settle(f.column_of(body));
    Rect field;
    REQUIRE(f.context.layout_rect(f.context.id("name"), field));
    f.driver.click(rect_centre(field), f.column_of(body));
    f.driver.run_frames(4, f.column_of(body));
    const MemoryStats before = test::all_allocations();
    f.driver.type("x");
    f.driver.frame(f.column_of(body));
    f.driver.run_frames(3, f.column_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
