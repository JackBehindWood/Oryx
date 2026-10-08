#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

struct ModalScene
{
    GuiFixture f;
    ImId under = f.context.id("under");
    ItemState under_state;
    gui::ModalChoice choice = gui::ModalChoice::None;
    bool opened = false;
    bool body_ran = false;

    void build_custom()
    {
        under_state = f.context.item(under, { { 0.0f, 0.0f }, { 200.0f, 100.0f } });
        opened = gui::begin_modal("Dialog");
        if (opened)
        {
            body_ran = true;
            gui::label("custom");
            gui::end_modal();
        }
    }

    void build_confirm()
    {
        under_state = f.context.item(under, { { 0.0f, 0.0f }, { 200.0f, 100.0f } });
        choice = gui::confirm("Switch", "Progress is lost.", "Switch", "Cancel");
    }

    Vec2f centre_of(std::string_view text) const
    {
        const LayoutNode* node = f.find_text(text);
        REQUIRE(node != nullptr);
        return rect_centre(node->rect);
    }
};

} // namespace

TEST_CASE("GUI modal: closed by default draws nothing and reports no popup")
{
    ModalScene s;
    s.f.driver.run_frames(3, [&] { s.build_custom(); });
    CHECK_FALSE(s.opened);
    CHECK_FALSE(s.f.context.popup_open());
    CHECK(s.f.find_text("custom") == nullptr);
}

TEST_CASE("GUI modal: open draws the dialog and reports popup_open")
{
    ModalScene s;
    s.f.driver.frame([&] { gui::open_modal("Dialog"); });
    s.f.driver.run_frames(3, [&] { s.build_custom(); });
    CHECK(s.opened);
    CHECK(s.body_ran);
    CHECK(s.f.find_text("custom") != nullptr);
    CHECK(s.f.context.popup_open());
}

TEST_CASE("GUI modal: items beneath are neither hovered nor clicked while it is open")
{
    ModalScene s;
    s.f.driver.frame([&] { gui::open_modal("Dialog"); });
    s.f.driver.move_to({ 5.0f, 5.0f });
    s.f.driver.run_frames(3, [&] { s.build_custom(); });
    CHECK_FALSE(s.under_state.hovered);
    bool clicked = false;
    s.f.driver.press();
    s.f.driver.frame([&] { s.build_custom(); clicked = clicked || s.under_state.clicked; });
    s.f.driver.release();
    s.f.driver.frame([&] { s.build_custom(); clicked = clicked || s.under_state.clicked; });
    CHECK_FALSE(clicked);
    CHECK(s.opened);
}

TEST_CASE("GUI modal: Escape closes it and an outside press does not")
{
    ModalScene s;
    s.f.driver.frame([&] { gui::open_modal("Dialog"); });
    s.f.driver.move_to({ 5.0f, 5.0f });
    s.f.driver.run_frames(3, [&] { s.build_custom(); });
    s.f.driver.press();
    s.f.driver.frame([&] { s.build_custom(); });
    s.f.driver.release();
    s.f.driver.run_frames(2, [&] { s.build_custom(); });
    CHECK(s.opened);
    s.f.driver.key_press(ImKey::Escape);
    s.f.driver.frame([&] { s.build_custom(); });
    CHECK_FALSE(s.opened);
    s.f.driver.run_frames(2, [&] { s.build_custom(); });
    CHECK_FALSE(s.opened);
    CHECK_FALSE(s.f.context.popup_open());
}

TEST_CASE("GUI modal: close_modal and close_current_modal both close it")
{
    ModalScene s;
    s.f.driver.frame([&] { gui::open_modal("Dialog"); });
    s.f.driver.run_frames(2, [&] { s.build_custom(); });
    s.f.driver.frame([&] { gui::close_modal("Dialog"); });
    s.f.driver.run_frames(2, [&] { s.build_custom(); });
    CHECK_FALSE(s.opened);
}

TEST_CASE("GUI modal: confirm returns Confirmed for its first button and closes")
{
    ModalScene s;
    s.f.driver.frame([&] { gui::open_modal("Switch"); });
    s.f.driver.run_frames(3, [&] { s.build_confirm(); });
    CHECK(s.choice == gui::ModalChoice::None);
    REQUIRE(s.f.find_text("Switch") != nullptr);
    gui::ModalChoice seen = gui::ModalChoice::None;
    const auto build = [&]
    {
        s.build_confirm();
        seen = s.choice != gui::ModalChoice::None ? s.choice : seen;
    };
    s.f.driver.click(s.centre_of("Switch"), build);
    CHECK(seen == gui::ModalChoice::Confirmed);
    seen = gui::ModalChoice::None;
    s.f.driver.run_frames(2, build);
    CHECK(seen == gui::ModalChoice::None);
    CHECK_FALSE(s.f.context.popup_open());
}

TEST_CASE("GUI modal: confirm returns Cancelled for the cancel button and for Escape")
{
    ModalScene s;
    gui::ModalChoice seen = gui::ModalChoice::None;
    const auto build = [&]
    {
        s.build_confirm();
        seen = s.choice != gui::ModalChoice::None ? s.choice : seen;
    };
    s.f.driver.frame([&] { gui::open_modal("Switch"); });
    s.f.driver.run_frames(3, build);
    s.f.driver.click(s.centre_of("Cancel"), build);
    CHECK(seen == gui::ModalChoice::Cancelled);

    seen = gui::ModalChoice::None;
    s.f.driver.frame([&] { gui::open_modal("Switch"); });
    s.f.driver.run_frames(3, build);
    s.f.driver.key_press(ImKey::Escape);
    s.f.driver.frame(build);
    CHECK(seen == gui::ModalChoice::Cancelled);
}

TEST_CASE("GUI modal: ModalScope matches the call form")
{
    GuiFixture f;
    f.driver.frame([&] { gui::open_modal("Dialog"); });
    bool open = false;
    f.driver.run_frames(3, [&]
    {
        gui::ModalScope modal("Dialog");
        open = modal.open();
        if (open)
        {
            gui::label("custom");
        }
    });
    CHECK(open);
    CHECK(f.find_text("custom") != nullptr);
}

TEST_CASE("GUI modal: a warm open frame allocates nothing")
{
    ModalScene s;
    s.f.driver.frame([&] { gui::open_modal("Switch"); });
    s.f.driver.run_frames(4, [&] { s.build_confirm(); });
    const MemoryStats before = test::all_allocations();
    s.f.driver.frame([&] { s.build_confirm(); });
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
