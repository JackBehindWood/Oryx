#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

struct MenuScene
{
    GuiFixture f;
    uint32_t opened = 0;
    uint32_t undone = 0;
    uint32_t recent = 0;

    MenuScene() { f.driver.input().surface_size = { 240.0f, 200.0f }; }

    void body()
    {
        gui::MenuBarScope bar("bar");
        {
            gui::MenuScope file("File");
            if (file.open())
            {
                opened += gui::menu_item("Open").clicked ? 1 : 0;
                gui::MenuScope more("Recent");
                if (more.open())
                {
                    recent += gui::menu_item("one").clicked ? 1 : 0;
                }
            }
        }
        gui::MenuScope edit("Edit");
        if (edit.open())
        {
            undone += gui::menu_item("Undo").clicked ? 1 : 0;
        }
    }

    void settle() { f.driver.settle(f.column_of([this] { body(); })); }
    void click_on(std::string_view text) { f.driver.click(rect_centre(f.find_text(text)->rect), f.column_of([this] { body(); })); }
    void hover(std::string_view text)
    {
        f.driver.move_to(rect_centre(f.find_text(text)->rect));
        f.driver.run_frames(2, f.column_of([this] { body(); }));
    }
    [[nodiscard]] bool shown(std::string_view text) const { return f.find_text(text) != nullptr; }
};

} // namespace

TEST_CASE("GUI menus: a click opens a menu, hovering a sibling switches, a pick fires and closes everything")
{
    MenuScene s;
    s.settle();
    CHECK_FALSE(s.shown("Open"));
    s.click_on("File");
    s.settle();
    CHECK(s.shown("Open"));
    CHECK_FALSE(s.shown("Undo"));
    s.hover("Edit");
    s.settle();
    CHECK_FALSE(s.shown("Open"));
    CHECK(s.shown("Undo"));
    s.click_on("Undo");
    CHECK(s.undone == 1);
    s.f.driver.run_frames(2, s.f.column_of([&] { s.body(); }));
    CHECK_FALSE(s.shown("Undo"));
}

TEST_CASE("GUI menus: a submenu opens on hover and a sibling item closes it")
{
    MenuScene s;
    s.settle();
    s.click_on("File");
    s.settle();
    s.hover("Recent  >");
    s.settle();
    REQUIRE(s.shown("one"));
    s.hover("Open");
    s.settle();
    CHECK_FALSE(s.shown("one"));
    s.hover("Recent  >");
    s.settle();
    s.click_on("one");
    CHECK(s.recent == 1);
}

TEST_CASE("GUI menus: Escape closes the innermost menu, a press outside closes the rest")
{
    MenuScene s;
    s.settle();
    s.click_on("File");
    s.settle();
    s.hover("Recent  >");
    s.settle();
    REQUIRE(s.shown("one"));
    s.hover("one");
    s.f.driver.key_press(ImKey::Escape);
    s.f.driver.run_frames(2, s.f.column_of([&] { s.body(); }));
    s.settle();
    CHECK_FALSE(s.shown("one"));
    CHECK(s.shown("Open"));
    s.f.driver.click({ 230.0f, 190.0f }, s.f.column_of([&] { s.body(); }));
    s.settle();
    CHECK_FALSE(s.shown("Open"));
}

TEST_CASE("GUI menus: a context menu opens on a right click and closes on a pick or an outside press")
{
    GuiFixture f;
    f.driver.input().surface_size = { 240.0f, 200.0f };
    uint32_t picked = 0;
    const auto body = [&]
    {
        const ItemState target = gui::button("target");
        if (gui::context_menu(target, "ctx"))
        {
            if (gui::button("act").clicked)
            {
                picked += 1;
                gui::close_current_popup();
            }
            gui::end_popup();
        }
    };
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("act") == nullptr);
    f.driver.click(rect_centre(f.find_text("target")->rect), f.column_of(body), MouseCode::Right);
    f.driver.settle(f.column_of(body));
    REQUIRE(f.find_text("act") != nullptr);
    f.driver.click(rect_centre(f.find_text("act")->rect), f.column_of(body));
    f.driver.settle(f.column_of(body));
    CHECK(picked == 1);
    CHECK(f.find_text("act") == nullptr);

    f.driver.click(rect_centre(f.find_text("target")->rect), f.column_of(body), MouseCode::Right);
    f.driver.settle(f.column_of(body));
    REQUIRE(f.find_text("act") != nullptr);
    f.driver.click({ 230.0f, 190.0f }, f.column_of(body));
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("act") == nullptr);
}

TEST_CASE("GUI menus: a tooltip appears after the delay on its own channel and never takes the hover")
{
    GuiFixture f;
    ImId hot_when_shown;
    const auto body = [&]
    {
        const ItemState target = gui::button("target");
        gui::tooltip(target, "hint");
    };
    f.driver.settle(f.column_of(body));
    f.driver.move_to(rect_centre(f.find_text("target")->rect));
    f.driver.run_frames(10, f.column_of(body));
    CHECK(f.find_text("hint") == nullptr);
    f.driver.run_frames(40, f.column_of(body));
    const LayoutNode* hint = f.find_text("hint");
    REQUIRE(hint != nullptr);
    CHECK(hint->style.channel == k_channel_tooltip);
    CHECK(f.context.hot() == f.context.id("target"));
    f.driver.move_to({ 190.0f, 90.0f });
    f.driver.run_frames(2, f.column_of(body));
    CHECK(f.find_text("hint") == nullptr);
}

TEST_CASE("GUI menus: toasts show for their time and then go")
{
    GuiFixture f;
    const auto body = [&] { gui::show_toasts(); };
    f.driver.frame(f.column_of([&] { gui::toast("saved", 0.05f); }));
    f.driver.run_frames(1, f.column_of(body));
    CHECK(f.find_text("saved") != nullptr);
    f.driver.run_frames(6, f.column_of(body));
    CHECK(f.find_text("saved") == nullptr);
}

TEST_CASE("GUI menus: the inspector reports the item under the pointer")
{
    GuiFixture f;
    f.driver.input().surface_size = { 240.0f, 300.0f };
    const auto body = [&]
    {
        std::ignore = gui::button("target");
        gui::inspector();
    };
    f.driver.settle(f.column_of(body));
    f.driver.move_to(rect_centre(f.find_text("target")->rect));
    f.driver.run_frames(3, f.column_of(body));
    const LayoutNode* hovered = f.find_text(f.context.arena().format("%016llx", static_cast<unsigned long long>(f.context.id("target").value)));
    CHECK(hovered != nullptr);
    CHECK(f.find_text("items") != nullptr);
}

TEST_CASE("GUI menus: warm frames with a menu, popup, tooltip and toast allocate nothing")
{
    MenuScene s;
    s.settle();
    s.click_on("File");
    s.settle();
    s.f.driver.move_to({ 5.0f, 5.0f });
    const auto body = [&]
    {
        s.body();
        const ItemState target = gui::button("target");
        gui::tooltip(target, "hint", 0.0f);
        gui::toast("note", 100.0f);
        gui::show_toasts();
        gui::inspector();
    };
    s.f.driver.run_frames(6, s.f.column_of(body));
    const MemoryStats before = test::all_allocations();
    s.f.driver.run_frames(3, s.f.column_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
