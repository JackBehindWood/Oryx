#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::fixed_box;
using test::GuiFixture;

namespace
{

struct Counter
{
    int32_t value = 0;
};

} // namespace

TEST_CASE("GUI: item_drag starts past the threshold and reports delta, total and the end")
{
    GuiFixture f;
    const ImId id = f.context.id("drag");
    ItemDrag last;
    uint32_t started = 0;
    uint32_t ended = 0;
    float travelled = 0.0f;
    const auto build = [&]
    {
        std::ignore = f.context.item(id, { { 10.0f, 10.0f }, { 40.0f, 40.0f } });
        last = f.context.item_drag(id);
        started += last.started ? 1 : 0;
        ended += last.ended ? 1 : 0;
        travelled += last.delta[0];
    };
    f.driver.drag({ 20.0f, 20.0f }, { 40.0f, 20.0f }, 4, build);
    CHECK(started == 1);
    CHECK(ended == 1);
    CHECK(travelled == doctest::Approx(20.0f));
    CHECK(last.total[0] == doctest::Approx(20.0f));
    CHECK(last.start[0] == doctest::Approx(20.0f));
}

TEST_CASE("GUI: a press that stays under the threshold is a click, not a drag")
{
    GuiFixture f;
    const ImId id = f.context.id("click");
    bool dragged = false;
    bool clicked = false;
    f.driver.click({ 20.0f, 20.0f }, [&]
    {
        const ItemState state = f.context.item(id, { { 10.0f, 10.0f }, { 40.0f, 40.0f } });
        const ItemDrag drag = f.context.item_drag(id);
        dragged = dragged || drag.started || drag.dragging || drag.ended;
        clicked = clicked || state.clicked;
    });
    CHECK(clicked);
    CHECK_FALSE(dragged);
}

TEST_CASE("GUI: item_drag is empty for an item that was not pressed")
{
    GuiFixture f;
    const ImId a = f.context.id("a");
    const ImId b = f.context.id("b");
    ItemDrag other;
    f.driver.drag({ 20.0f, 20.0f }, { 60.0f, 20.0f }, 3, [&]
    {
        std::ignore = f.context.item(a, { { 10.0f, 10.0f }, { 40.0f, 40.0f } });
        std::ignore = f.context.item(b, { { 100.0f, 10.0f }, { 40.0f, 40.0f } });
        other = f.context.item_drag(b);
        CHECK_FALSE(other.dragging);
    });
}

TEST_CASE("GUI: double and right clicks")
{
    GuiFixture f;
    const ImId id = f.context.id("target");
    uint32_t clicks = 0;
    uint32_t doubles = 0;
    uint32_t rights = 0;
    const auto build = [&]
    {
        const ItemState state = f.context.item(id, { { 10.0f, 10.0f }, { 40.0f, 40.0f } });
        clicks += state.clicked ? 1 : 0;
        doubles += state.double_clicked ? 1 : 0;
        rights += state.right_clicked ? 1 : 0;
    };
    f.driver.click({ 20.0f, 20.0f }, build);
    CHECK(clicks == 1);
    CHECK(doubles == 0);
    f.driver.click({ 20.0f, 20.0f }, build);
    CHECK(clicks == 2);
    CHECK(doubles == 1);
    f.driver.click({ 20.0f, 20.0f }, build);
    CHECK(doubles == 1);
    f.driver.click({ 20.0f, 20.0f }, build, MouseCode::Right);
    CHECK(rights == 1);
    CHECK(clicks == 3);
}

TEST_CASE("GUI: clicks further apart than the double-click time are two single clicks")
{
    GuiFixture f;
    const ImId id = f.context.id("slow");
    uint32_t doubles = 0;
    const auto build = [&] { doubles += f.context.item(id, { { 10.0f, 10.0f }, { 40.0f, 40.0f } }).double_clicked ? 1 : 0; };
    f.driver.click({ 20.0f, 20.0f }, build);
    for (uint32_t index = 0; index < 40; ++index)
    {
        f.driver.frame(build);
    }
    f.driver.click({ 20.0f, 20.0f }, build);
    CHECK(doubles == 0);
}

TEST_CASE("GUI: state<T> persists, is collected when unused and checks its type")
{
    GuiFixture f;
    const ImId id = f.context.id("counter");
    f.driver.frame([&] { f.context.state<Counter>(id).value = 7; });
    f.driver.frame([&] { CHECK(f.context.state<Counter>(id).value == 7); });
    f.driver.frame([&] {});
    f.driver.frame([&] { CHECK(f.context.state<Counter>(id).value == 0); });
    f.driver.frame([&] { CHECK_THROWS_AS(std::ignore = f.context.state<float>(id), Error); });
    CHECK_THROWS_AS(std::ignore = f.context.state<Counter>(id), Error);
}

TEST_CASE("GUI: state<T> allocates only when a new id appears")
{
    GuiFixture f;
    const ImId id = f.context.id("warm");
    const auto build = [&] { f.context.state<Counter>(id).value += 1; };
    f.driver.frame(build);
    f.driver.frame(build);
    const MemoryStats before = test::all_allocations();
    f.driver.frame(build);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("GUI: canvas hands out the rect, the local pointer and clipped painting above the box paint")
{
    GuiFixture f;
    Rect rect;
    Vec2f local;
    const auto build = [&]
    {
        LayoutStyle panel = fixed_box(100.0f, 60.0f);
        panel.padding = { 10.0f, 10.0f, 10.0f, 10.0f };
        gui::PanelScope scope("panel", { .layout = &panel });
        im::CanvasArea area = gui::canvas("view", grow(), grow());
        rect = area.rect;
        local = area.pointer_local;
        area.painter.fill_rect(area.rect, { 1.0f, 0.0f, 0.0f, 1.0f });
    };
    f.driver.move_to({ 30.0f, 25.0f });
    f.driver.settle(f.frame_of(build));
    CHECK(rect.min[0] == doctest::Approx(10.0f));
    CHECK(rect.size[0] == doctest::Approx(80.0f));
    CHECK(local[0] == doctest::Approx(20.0f));
    CHECK(local[1] == doctest::Approx(15.0f));

    const DrawChannel& channel = f.context.draw_list().channel(0);
    REQUIRE(f.context.draw_list().channel_count() == 1);
    CHECK(channel.rects.size() >= 1);
    const std::string text = dump(f.context.draw_list());
    CHECK(text.find("rect") != std::string::npos);
}

TEST_CASE("GUI: a draggable rectangle and a range selector written as user code on canvas and item_drag")
{
    GuiFixture f;
    Vec2f position{ 20.0f, 20.0f };
    float low = 0.2f;
    float high = 0.8f;
    const auto build = [&]
    {
        im::CanvasArea area = gui::canvas("scene", grow(), grow());
        const Rect box = { position, { 30.0f, 30.0f } };
        const ImId handle = f.context.id("scene/handle");
        std::ignore = f.context.item(handle, box);
        const ItemDrag drag = f.context.item_drag(handle);
        position = position + drag.delta;
        area.painter.fill_rect(box, { 0.0f, 1.0f, 0.0f, 1.0f });

        const ImId low_id = f.context.id("scene/low");
        const float low_x = area.rect.min[0] + low * area.rect.size[0];
        std::ignore = f.context.item(low_id, { { low_x - 5.0f, 80.0f }, { 10.0f, 15.0f } });
        const ItemDrag low_drag = f.context.item_drag(low_id);
        if (area.rect.size[0] > 0.0f)
        {
            low = std::clamp(low + low_drag.delta[0] / area.rect.size[0], 0.0f, high);
        }
    };
    f.driver.settle(f.frame_of(build));
    f.driver.drag({ 30.0f, 30.0f }, { 50.0f, 40.0f }, 5, f.frame_of(build));
    CHECK(position[0] == doctest::Approx(40.0f));
    CHECK(position[1] == doctest::Approx(30.0f));
    f.driver.drag({ 40.0f, 88.0f }, { 100.0f, 88.0f }, 5, f.frame_of(build));
    CHECK(low == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE("GUI: gui functions throw without an active context")
{
    CHECK_THROWS_AS(gui::io(), Error);
}

TEST_CASE("GUI: the UI and GUI contexts have independent active slots and ids")
{
    GuiFixture f;
    CHECK(ActiveContext<UiContext>::get() == nullptr);
    UiContext ui_context;
    {
        ContextScope<UiContext> scope(ui_context);
        CHECK(ActiveContext<UiContext>::get() == &ui_context);
        CHECK(ActiveContext<GuiContext>::get() == &f.context);
    }
    CHECK(ActiveContext<UiContext>::get() == nullptr);
    f.context.begin_frame({});
    ui_context.begin_frame({});
    CHECK(f.context.id("x") != ui_context.id("x"));
    ui_context.end_frame();
    f.context.end_frame();
}

TEST_CASE("GUI: gui::io reads the frame and the cursor request resets each frame")
{
    GuiFixture f;
    f.driver.move_to({ 20.0f, 20.0f });
    const ImId id = f.context.id("io");
    f.driver.frame([&]
    {
        std::ignore = f.context.item(id, { { 10.0f, 10.0f }, { 40.0f, 40.0f } });
        gui::request_cursor(CursorShape::ResizeHorizontal);
    });
    CHECK(f.context.output().cursor == CursorShape::ResizeHorizontal);
    f.driver.frame([&]
    {
        const gui::GuiIo io = gui::io();
        CHECK(io.frame == 2);
        CHECK(io.pointer.valid);
        CHECK(io.delta_time == doctest::Approx(1.0f / 60.0f));
    });
    CHECK(f.context.output().cursor == CursorShape::Arrow);
}

TEST_CASE("GUI: wrappers mirror the UI signatures")
{
    static_assert(std::is_same_v<decltype(&gui::label), decltype(&ui::label)>);
    static_assert(std::is_same_v<decltype(&gui::button), decltype(&ui::button)>);
    static_assert(std::is_same_v<decltype(&gui::toggle), decltype(&ui::toggle)>);
    static_assert(std::is_same_v<decltype(&gui::begin_panel), decltype(&ui::begin_panel)>);
    static_assert(std::is_same_v<decltype(&gui::end_panel), decltype(&ui::end_panel)>);
    static_assert(std::is_same_v<decltype(&gui::begin_row), decltype(&ui::begin_row)>);
    static_assert(std::is_same_v<decltype(&gui::end_row), decltype(&ui::end_row)>);
    static_assert(std::is_same_v<decltype(&gui::begin_column), decltype(&ui::begin_column)>);
    static_assert(std::is_same_v<decltype(&gui::end_column), decltype(&ui::end_column)>);
    static_assert(std::is_same_v<decltype(&gui::spacer), decltype(&ui::spacer)>);
    static_assert(std::is_same_v<decltype(&gui::separator), decltype(&ui::separator)>);
    static_assert(std::is_same_v<decltype(&gui::status_line), decltype(&ui::status_line)>);
    static_assert(std::is_same_v<decltype(&gui::begin_box), decltype(&ui::begin_box)>);
    static_assert(std::is_same_v<decltype(&gui::end_box), decltype(&ui::end_box)>);
    static_assert(std::is_same_v<decltype(&gui::anchored), decltype(&ui::anchored)>);
    static_assert(!std::is_convertible_v<UiId, GuiId>);
}

TEST_CASE("GUI: gui widgets draw like the ui ones and stay allocation free once warm")
{
    GuiFixture f;
    bool on = false;
    const auto build = [&]
    {
        gui::PanelScope panel("p");
        gui::label("Name");
        std::ignore = gui::button("Go");
        gui::toggle("Flag", on);
        im::CanvasArea area = gui::canvas("c", fixed(50.0f), fixed(20.0f));
        area.painter.fill_rect(area.rect, { 1.0f, 1.0f, 1.0f, 1.0f });
    };
    f.driver.settle(f.frame_of(build));
    f.driver.frame(f.frame_of(build));
    f.driver.frame(f.frame_of(build));
    const MemoryStats before = test::all_allocations();
    f.driver.frame(f.frame_of(build));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
