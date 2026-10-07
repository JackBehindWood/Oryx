#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

LayoutStyle popup_style(const Vec2f& at, float width = 60.0f, float height = 40.0f)
{
    LayoutStyle style;
    style.width = fixed(width);
    style.height = fixed(height);
    style.channel = k_channel_popup;
    style.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Root, {}, at };
    return style;
}

struct PopupScene
{
    GuiFixture f;
    ImId under = f.context.id("under");
    ImId popup = f.context.id("popup");
    ImId inside = f.context.id("inside");
    bool open = true;
    ItemState under_state;
    ItemState inside_state;
    PopupResult result;

    void build()
    {
        under_state = f.context.item(under, { { 10.0f, 10.0f }, { 120.0f, 80.0f } });
        if (open)
        {
            result = f.context.begin_popup_layer(popup);
            f.context.begin_box(popup, popup_style({ 20.0f, 20.0f }));
            inside_state = f.context.item(inside, { { 20.0f, 20.0f }, { 60.0f, 40.0f } });
            f.context.end_box();
            f.context.end_popup_layer();
        }
    }
};

} // namespace

TEST_CASE("GUI popup: hovered_seconds counts consecutive hovered frames and resets")
{
    GuiFixture f;
    const ImId id = f.context.id("hover");
    float seconds = -1.0f;
    const auto build = [&] { seconds = f.context.item(id, { { 0.0f, 0.0f }, { 50.0f, 50.0f } }).hovered_seconds; };
    f.driver.move_to({ 10.0f, 10.0f });
    f.driver.frame(build);
    CHECK(seconds == doctest::Approx(0.0f));
    f.driver.run_frames(3, build);
    CHECK(seconds == doctest::Approx(3.0f / 60.0f));
    f.driver.move_to({ 150.0f, 90.0f });
    f.driver.frame(build);
    CHECK(seconds == doctest::Approx(0.0f));
    f.driver.move_to({ 10.0f, 10.0f });
    f.driver.frame(build);
    CHECK(seconds == doctest::Approx(0.0f));
}

TEST_CASE("GUI popup: items under an open popup are not hovered, the popup's own items are")
{
    PopupScene s;
    s.f.driver.move_to({ 30.0f, 30.0f });
    s.f.driver.run_frames(3, [&] { s.build(); });
    CHECK_FALSE(s.under_state.hovered);
    CHECK(s.inside_state.hovered);
    s.f.driver.move_to({ 100.0f, 80.0f });
    s.f.driver.run_frames(2, [&] { s.build(); });
    CHECK(s.under_state.hovered);
    s.open = false;
    s.f.driver.move_to({ 30.0f, 30.0f });
    s.f.driver.run_frames(3, [&] { s.build(); });
    CHECK(s.under_state.hovered);
}

TEST_CASE("GUI popup: a press outside reports closed_by_outside, a press inside does not")
{
    PopupScene s;
    s.f.driver.move_to({ 30.0f, 30.0f });
    s.f.driver.run_frames(2, [&] { s.build(); });
    s.f.driver.press();
    s.f.driver.frame([&] { s.build(); });
    CHECK_FALSE(s.result.closed_by_outside);
    s.f.driver.release();
    s.f.driver.frame([&] { s.build(); });
    s.f.driver.move_to({ 150.0f, 90.0f });
    s.f.driver.frame([&] { s.build(); });
    CHECK_FALSE(s.result.closed_by_outside);
    s.f.driver.press();
    s.f.driver.frame([&] { s.build(); });
    CHECK(s.result.closed_by_outside);
    CHECK_FALSE(s.result.closed_by_escape);
}

TEST_CASE("GUI popup: Escape closes only the innermost layer")
{
    GuiFixture f;
    const ImId outer = f.context.id("outer");
    const ImId inner = f.context.id("inner");
    PopupResult outer_result;
    PopupResult inner_result;
    const auto build = [&]
    {
        outer_result = f.context.begin_popup_layer(outer);
        f.context.begin_box(outer, popup_style({ 10.0f, 10.0f }));
        inner_result = f.context.begin_popup_layer(inner);
        f.context.begin_box(inner, popup_style({ 80.0f, 10.0f }));
        f.context.end_box();
        f.context.end_popup_layer();
        f.context.end_box();
        f.context.end_popup_layer();
    };
    f.driver.run_frames(2, build);
    f.driver.key_press(ImKey::Escape);
    f.driver.frame(build);
    CHECK(inner_result.closed_by_escape);
    CHECK_FALSE(outer_result.closed_by_escape);
}

TEST_CASE("GUI popup: a press inside a nested layer does not close its parent")
{
    GuiFixture f;
    const ImId outer = f.context.id("outer");
    const ImId inner = f.context.id("inner");
    PopupResult outer_result;
    const auto build = [&]
    {
        outer_result = f.context.begin_popup_layer(outer);
        f.context.begin_box(outer, popup_style({ 10.0f, 10.0f }));
        std::ignore = f.context.begin_popup_layer(inner);
        f.context.begin_box(inner, popup_style({ 100.0f, 10.0f }));
        f.context.end_box();
        f.context.end_popup_layer();
        f.context.end_box();
        f.context.end_popup_layer();
    };
    f.driver.run_frames(2, build);
    f.driver.move_to({ 120.0f, 20.0f });
    f.driver.press();
    f.driver.frame(build);
    CHECK_FALSE(outer_result.closed_by_outside);
}

TEST_CASE("GUI popup: the stack throws past its depth and on an unbalanced end")
{
    GuiFixture f;
    f.context.begin_frame(f.driver.input());
    for (uint32_t depth = 0; depth < k_max_popup_depth; ++depth)
    {
        std::ignore = f.context.begin_popup_layer(f.context.index_id(depth));
    }
    CHECK_THROWS_AS(std::ignore = f.context.begin_popup_layer(f.context.index_id(99)), Error);
    CHECK_THROWS_AS(f.context.end_frame(), Error);
    f.context.abort_frame();
    f.context.begin_frame(f.driver.input());
    CHECK_THROWS_AS(f.context.end_popup_layer(), Error);
    f.context.abort_frame();
}

TEST_CASE("GUI popup: a floating box on the surface escapes its parent's clip")
{
    GuiFixture f;
    const auto build = [&]
    {
        LayoutStyle clipped;
        clipped.width = fixed(40.0f);
        clipped.height = fixed(40.0f);
        clipped.overflow = Overflow::Clip;
        f.context.begin_box("clipped", clipped);
        f.context.begin_box("popup", popup_style({ 100.0f, 50.0f }));
        f.context.end_box();
        f.context.end_box();
    };
    f.driver.settle(build);
    Rect clip;
    CHECK(f.context.layout().clip_of(f.context.id("popup"), clip));
    CHECK(clip.size[0] > 1.0e6f);
}

TEST_CASE("GUI popup: placement flips at the surface edges using last frame's size")
{
    const Rect anchor{ { 160.0f, 80.0f }, { 40.0f, 16.0f } };
    const Vec2f surface(200.0f, 100.0f);
    const ImId id{ 7 };
    const Floating fits = im::popup_below(id, { { 10.0f, 10.0f }, { 40.0f, 16.0f } }, { 50.0f, 30.0f }, surface);
    CHECK(fits.element == AttachPoint::TopLeft);
    CHECK(fits.target_point == AttachPoint::BottomLeft);
    const Floating flipped = im::popup_below(id, anchor, { 50.0f, 30.0f }, surface);
    CHECK(flipped.element == AttachPoint::BottomRight);
    CHECK(flipped.target_point == AttachPoint::TopRight);
    const Floating unknown = im::popup_below(id, anchor, { 0.0f, 0.0f }, surface);
    CHECK(unknown.element == AttachPoint::TopLeft);
    const Floating near = im::popup_at({ 20.0f, 20.0f }, { 50.0f, 30.0f }, surface, 12.0f);
    CHECK(near.offset[0] == doctest::Approx(32.0f));
    const Floating far = im::popup_at({ 190.0f, 95.0f }, { 50.0f, 30.0f }, surface, 12.0f);
    CHECK(far.offset[0] == doctest::Approx(128.0f));
    CHECK(far.offset[1] == doctest::Approx(53.0f));
}

TEST_CASE("GUI popup: a warm popup frame allocates nothing")
{
    PopupScene s;
    s.f.driver.move_to({ 30.0f, 30.0f });
    s.f.driver.run_frames(3, [&] { s.build(); });
    const MemoryStats before = test::all_allocations();
    s.f.driver.run_frames(3, [&] { s.build(); });
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
