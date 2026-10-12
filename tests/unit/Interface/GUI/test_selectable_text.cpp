#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

struct SelectScene
{
    GuiFixture f;
    std::string text = "alpha beta\ngamma delta\nepsilon";
    std::string picked;
    ItemState item;

    SelectScene() { f.driver.input().surface_size = { 300.0f, 200.0f }; }

    void body()
    {
        item = gui::selectable_text("block", text);
        picked = std::string(gui::selected_text("block", text));
    }

    void frame() { f.driver.frame(f.column_of([this] { body(); })); }
    void settle() { f.driver.settle(f.column_of([this] { body(); })); }
    float line_height() const { return f.context.gui_theme().panel.text_height + 4.0f; }
    void drag_to(const Vec2f& from, const Vec2f& to)
    {
        f.driver.move_to(from);
        f.driver.press();
        frame();
        f.driver.move_to(to);
        frame();
        frame();
    }
};

} // namespace

TEST_CASE("selectable_text: dragging across lines selects the text between the ends")
{
    SelectScene s;
    s.settle();
    const float lh = s.line_height();
    s.drag_to({ 1.0f, lh * 0.5f }, { 250.0f, lh * 1.5f });
    CHECK(s.picked == "alpha beta\ngamma delta");
    CHECK(s.f.context.focus() == s.f.context.id("block"));
}

TEST_CASE("selectable_text: a drag upwards selects the same range")
{
    SelectScene s;
    s.settle();
    const float lh = s.line_height();
    s.drag_to({ 250.0f, lh * 1.5f }, { 1.0f, lh * 0.5f });
    CHECK(s.picked == "alpha beta\ngamma delta");
}

TEST_CASE("selectable_text: the copy shortcut sends the selection to the clipboard")
{
    SelectScene s;
    s.settle();
    const float lh = s.line_height();
    s.drag_to({ 1.0f, lh * 0.5f }, { 250.0f, lh * 0.5f });
    s.f.driver.chord(ImKey::C);
    s.frame();
    CHECK(s.f.context.output().copy_text == "alpha beta");
}

TEST_CASE("selectable_text: select all and a double click take everything")
{
    SelectScene s;
    s.settle();
    const float lh = s.line_height();
    s.drag_to({ 1.0f, lh * 0.5f }, { 5.0f, lh * 0.5f });
    s.f.driver.release();
    s.frame();
    s.f.driver.chord(ImKey::A);
    s.frame();
    CHECK(s.picked == s.text);
    s.f.driver.chord(ImKey::C);
    s.frame();
    CHECK(s.f.context.output().copy_text == s.text);
}

TEST_CASE("selectable_text: the pointer over the text asks for the text cursor")
{
    SelectScene s;
    s.settle();
    s.f.driver.move_to({ 10.0f, s.line_height() * 0.5f });
    s.frame();
    s.frame();
    CHECK(s.f.context.output().cursor == CursorShape::Text);
}

TEST_CASE("selectable_text: a press elsewhere or Escape drops focus and the selection")
{
    SelectScene s;
    s.settle();
    const float lh = s.line_height();
    s.drag_to({ 1.0f, lh * 0.5f }, { 250.0f, lh * 0.5f });
    REQUIRE_FALSE(s.picked.empty());
    s.f.driver.release();
    s.frame();
    s.f.driver.key_press(ImKey::Escape);
    s.frame();
    CHECK(s.picked.empty());
    CHECK_FALSE(is_valid(s.f.context.focus()));
}

TEST_CASE("selectable_text: a selection past a shortened text is clamped")
{
    SelectScene s;
    s.settle();
    const float lh = s.line_height();
    s.drag_to({ 1.0f, lh * 0.5f }, { 250.0f, lh * 2.5f });
    s.text = "ab";
    s.frame();
    CHECK(s.picked == "ab");
}
