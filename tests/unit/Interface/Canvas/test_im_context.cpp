#include "doctest.h"

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

struct TestContext : ImContext
{
};

const Rect k_button = { { 10.0f, 10.0f }, { 40.0f, 20.0f } };
const Rect k_other = { { 100.0f, 10.0f }, { 40.0f, 20.0f } };

ImInput pointer_at(float x, float y)
{
    ImInput input;
    input.pointer.valid = true;
    input.pointer.position = { x, y };
    return input;
}

ImInput with_left(ImInput input, bool down, bool pressed, bool released)
{
    input.pointer.buttons[static_cast<uint32_t>(MouseCode::Left)] = { down, pressed, released };
    return input;
}

ItemState frame_with_item(TestContext& context, const ImInput& input, ImId id, const Rect& rect)
{
    context.begin_frame(input);
    ItemState state = context.item(id, rect);
    context.end_frame();
    return state;
}

} // namespace

TEST_CASE("ImContext: frame misuse throws")
{
    TestContext context;
    CHECK_THROWS_AS(context.end_frame(), Error);
    CHECK_THROWS_AS(context.push_id("x"), Error);
    CHECK_THROWS_AS(context.item(make_im_id("x"), k_button), Error);
    context.begin_frame({});
    CHECK_THROWS_AS(context.begin_frame({}), Error);
    CHECK_THROWS_AS(context.pop_id(), Error);
    CHECK_THROWS_AS(context.item(ImId{}, k_button), Error);
    context.end_frame();
    CHECK(context.frame() == 1);
}

TEST_CASE("ImContext: an unbalanced id or clip scope fails end_frame and abort_frame recovers")
{
    TestContext context;
    context.begin_frame({});
    context.push_id("panel");
    CHECK_THROWS_AS(context.end_frame(), Error);
    context.abort_frame();
    context.begin_frame({});
    context.draw_list().push_clip(k_button);
    CHECK_THROWS_AS(context.end_frame(), Error);
    context.abort_frame();
    CHECK_NOTHROW(context.begin_frame({}));
    CHECK(context.draw_list().clip_depth() == 0);
    context.end_frame();
}

TEST_CASE("ImContext: ids nest through scopes and the same label differs per scope")
{
    TestContext context;
    context.begin_frame({});
    const ImId plain = context.id("OK");
    ImId scoped;
    {
        IdScope scope(context, "dialog");
        scoped = context.id("OK");
        CHECK(context.current_id() != ImId{});
    }
    CHECK(scoped != plain);
    CHECK(context.current_id() == ImId{});
    CHECK(context.id("OK") == plain);
    CHECK(context.index_id(2) != context.index_id(3));
    context.end_frame();
}

TEST_CASE("ImContext: an id used twice in a frame throws, but a scope makes it distinct")
{
    TestContext context;
    context.begin_frame({});
    context.item(context.id("OK"), k_button);
    CHECK_THROWS_AS(context.item(context.id("OK"), k_other), Error);
    {
        IdScope scope(context, "second");
        CHECK_NOTHROW(context.item(context.id("OK"), k_other));
    }
    context.end_frame();
    context.begin_frame({});
    CHECK_NOTHROW(context.item(context.id("OK"), k_button));
    context.end_frame();
}

TEST_CASE("ImContext: hover follows the pointer and a missing pointer hovers nothing")
{
    TestContext context;
    const ImId id = make_im_id("b");
    CHECK(frame_with_item(context, pointer_at(20.0f, 20.0f), id, k_button).hovered);
    CHECK_FALSE(frame_with_item(context, pointer_at(60.0f, 20.0f), id, k_button).hovered);
    ImInput away = pointer_at(20.0f, 20.0f);
    away.pointer.valid = false;
    CHECK_FALSE(frame_with_item(context, away, id, k_button).hovered);
}

TEST_CASE("ImContext: a click is press then release over the same item")
{
    TestContext context;
    const ImId id = make_im_id("b");
    ItemState down = frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), true, true, false), id, k_button);
    CHECK(down.pressed);
    CHECK(down.held);
    CHECK_FALSE(down.clicked);
    CHECK(context.active() == id);
    ItemState held = frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), true, false, false), id, k_button);
    CHECK_FALSE(held.pressed);
    CHECK(held.held);
    ItemState up = frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), false, false, true), id, k_button);
    CHECK(up.clicked);
    CHECK_FALSE(up.held);
    CHECK_FALSE(is_valid(context.active()));
}

TEST_CASE("ImContext: a press and release in one frame still clicks")
{
    TestContext context;
    ItemState state = frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), false, true, true), make_im_id("b"), k_button);
    CHECK(state.pressed);
    CHECK(state.clicked);
}

TEST_CASE("ImContext: pressing A and releasing over B is no click on either")
{
    TestContext context;
    const ImId a = make_im_id("a");
    const ImId b = make_im_id("b");
    auto frame = [&](const ImInput& input)
    {
        context.begin_frame(input);
        ItemState first = context.item(a, k_button);
        ItemState second = context.item(b, k_other);
        context.end_frame();
        return std::pair<ItemState, ItemState>(first, second);
    };
    frame(with_left(pointer_at(20.0f, 20.0f), true, true, false));
    auto [first, second] = frame(with_left(pointer_at(110.0f, 20.0f), false, false, true));
    CHECK_FALSE(first.clicked);
    CHECK_FALSE(second.clicked);
    CHECK_FALSE(is_valid(context.active()));
}

TEST_CASE("ImContext: dragging onto an item after pressing empty space is no click")
{
    TestContext context;
    const ImId id = make_im_id("b");
    frame_with_item(context, with_left(pointer_at(300.0f, 300.0f), true, true, false), id, k_button);
    ItemState state = frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), true, false, false), id, k_button);
    CHECK(state.hovered);
    CHECK_FALSE(state.pressed);
    state = frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), false, false, true), id, k_button);
    CHECK_FALSE(state.clicked);
}

TEST_CASE("ImContext: wants_mouse while hovering or holding, and a lost release clears active")
{
    TestContext context;
    const ImId id = make_im_id("b");
    context.begin_frame(pointer_at(300.0f, 300.0f));
    context.item(id, k_button);
    CHECK_FALSE(context.wants_mouse());
    context.end_frame();
    context.begin_frame(pointer_at(20.0f, 20.0f));
    context.item(id, k_button);
    CHECK(context.wants_mouse());
    CHECK(context.hot() == id);
    context.end_frame();
    context.begin_frame(with_left(pointer_at(20.0f, 20.0f), true, true, false));
    context.item(id, k_button);
    context.end_frame();
    CHECK(context.wants_mouse());
    context.begin_frame(pointer_at(300.0f, 300.0f));
    context.end_frame();
    CHECK_FALSE(is_valid(context.active()));
    CHECK_FALSE(context.wants_mouse());
}

TEST_CASE("ImContext: an item outside the clip is not hovered")
{
    TestContext context;
    context.begin_frame(pointer_at(20.0f, 20.0f));
    context.draw_list().push_clip({ { 0.0f, 0.0f }, { 15.0f, 15.0f } });
    CHECK_FALSE(context.item(make_im_id("clipped"), k_button).hovered);
    context.draw_list().pop_clip();
    CHECK(context.item(make_im_id("free"), { { 12.0f, 12.0f }, { 20.0f, 20.0f } }).hovered);
    context.end_frame();
}

TEST_CASE("ImContext: min_hit_size grows the hit area of a small item")
{
    TestContext context;
    ImTheme theme;
    theme.min_hit_size = 40.0f;
    context.set_theme(theme);
    const Rect tiny = { { 100.0f, 100.0f }, { 10.0f, 10.0f } };
    CHECK(frame_with_item(context, pointer_at(90.0f, 90.0f), make_im_id("tiny"), tiny).hovered);
    CHECK_FALSE(frame_with_item(context, pointer_at(60.0f, 60.0f), make_im_id("tiny"), tiny).hovered);
}

TEST_CASE("ImContext: focus is reserved, drives wants_keyboard and a press on nothing clears it")
{
    TestContext context;
    const ImId id = make_im_id("field");
    context.set_focus(id);
    CHECK(context.wants_keyboard());
    frame_with_item(context, with_left(pointer_at(20.0f, 20.0f), true, true, false), id, k_button);
    CHECK(context.focus() == id);
    context.begin_frame(with_left(pointer_at(300.0f, 300.0f), true, true, false));
    context.end_frame();
    CHECK_FALSE(context.wants_keyboard());
}

TEST_CASE("ImContext: the previous frame's rect is remembered by id")
{
    TestContext context;
    const ImId id = make_im_id("b");
    Rect rect;
    context.begin_frame({});
    CHECK_FALSE(context.previous_rect(id, rect));
    context.item(id, k_button);
    CHECK_FALSE(context.previous_rect(id, rect));
    context.end_frame();
    context.begin_frame({});
    REQUIRE(context.previous_rect(id, rect));
    CHECK(rect == k_button);
    context.item(id, k_other);
    REQUIRE(context.previous_rect(id, rect));
    CHECK(rect == k_button);
    context.end_frame();
    context.begin_frame({});
    REQUIRE(context.previous_rect(id, rect));
    CHECK(rect == k_other);
    context.end_frame();
    context.begin_frame({});
    context.end_frame();
    context.begin_frame({});
    CHECK_FALSE(context.previous_rect(id, rect));
    context.end_frame();
}

TEST_CASE("ImContext: input, surface, draw list and arena are per frame")
{
    TestContext context;
    ImInput input = pointer_at(1.0f, 2.0f);
    input.surface = 4;
    input.delta_time = 0.016f;
    context.begin_frame(input);
    CHECK(context.draw_list().surface() == 4);
    CHECK(context.delta_time() == doctest::Approx(0.016f));
    context.draw_list().add_rect(k_button, { 1.0f, 0.0f, 0.0f, 1.0f });
    std::ignore = context.arena().format("n=%d", 5);
    CHECK(context.stats().commands == 1);
    CHECK(context.stats().arena_used > 0);
    context.end_frame();
    context.begin_frame({});
    CHECK(context.draw_list().command_count() == 0);
    CHECK(context.stats().arena_used == 0);
    context.end_frame();
}

TEST_CASE("ImContext: painting needs a themed font")
{
    TestContext context;
    CHECK_THROWS_AS(context.painter(), Error);
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    ImTheme theme;
    theme.font = &font;
    context.set_theme(theme);
    context.begin_frame({});
    Painter painter = context.painter();
    painter.fill_rect(k_button, { 1.0f, 1.0f, 1.0f, 1.0f });
    CHECK(context.stats().commands == 1);
    context.end_frame();
}

TEST_CASE("ImContext: two contexts keep separate state")
{
    TestContext a;
    TestContext b;
    const ImId id = make_im_id("b");
    frame_with_item(a, with_left(pointer_at(20.0f, 20.0f), true, true, false), id, k_button);
    CHECK(a.active() == id);
    CHECK_FALSE(is_valid(b.active()));
    CHECK(b.frame() == 0);
}

TEST_CASE("ImContext: a warm frame with items allocates nothing")
{
    TestContext context;
    auto frame = [&context]()
    {
        context.begin_frame(with_left(pointer_at(20.0f, 20.0f), true, true, false));
        for (uint32_t i = 0; i < 30; ++i)
        {
            context.item(context.index_id(i), { { static_cast<float>(i) * 5.0f, 10.0f }, { 4.0f, 4.0f } });
            context.draw_list().add_rect(k_button, { 1.0f, 1.0f, 1.0f, 1.0f });
            std::ignore = context.arena().format("row %u", i);
        }
        context.end_frame();
    };
    frame();
    frame();
    MemoryStats before = test::all_allocations();
    frame();
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
