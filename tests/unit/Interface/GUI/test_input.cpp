#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using test::GuiFixture;

namespace
{

Rect track_of(const GuiFixture& f, std::string_view field)
{
    Rect rect;
    REQUIRE(f.context.layout_rect(make_im_id("track", f.context.id(field)), rect));
    return rect;
}

} // namespace

TEST_CASE("GUI input: a slider follows the pointer, clamps, and stops when released")
{
    GuiFixture f;
    float value = 0.0f;
    const auto body = [&] { std::ignore = gui::slider_float("speed", value, 0.0f, 10.0f); };
    f.driver.settle(f.column_of(body));
    const Rect track = track_of(f, "speed");
    const float y = rect_centre(track)[1];
    f.driver.move_to({ track.min[0] + track.size[0] * 0.5f, y });
    f.driver.frame(f.column_of(body));
    f.driver.press();
    f.driver.frame(f.column_of(body));
    CHECK(value == doctest::Approx(5.0f).epsilon(0.05));
    f.driver.move_to({ track.min[0] + track.size[0] + 500.0f, y });
    f.driver.frame(f.column_of(body));
    CHECK(value == doctest::Approx(10.0f));
    f.driver.release();
    f.driver.frame(f.column_of(body));
    f.driver.move_to({ track.min[0], y });
    f.driver.frame(f.column_of(body));
    CHECK(value == doctest::Approx(10.0f));
}

TEST_CASE("GUI input: arrow keys nudge a slider only while it is focused")
{
    GuiFixture f;
    float first = 5.0f;
    float second = 5.0f;
    const auto body = [&]
    {
        std::ignore = gui::slider_float("first", first, 0.0f, 10.0f);
        std::ignore = gui::slider_float("second", second, 0.0f, 10.0f);
    };
    f.driver.settle(f.column_of(body));
    f.driver.key_press(ImKey::Right);
    f.driver.frame(f.column_of(body));
    CHECK(first == doctest::Approx(5.0f));
    const Rect track = track_of(f, "first");
    f.driver.move_to({ track.min[0] + track.size[0] * 0.5f, rect_centre(track)[1] });
    f.driver.frame(f.column_of(body));
    f.driver.press();
    f.driver.frame(f.column_of(body));
    f.driver.release();
    f.driver.frame(f.column_of(body));
    const float settled = first;
    f.driver.key_press(ImKey::Right);
    f.driver.frame(f.column_of(body));
    CHECK(first == doctest::Approx(settled + 0.1f));
    CHECK(second == doctest::Approx(5.0f));
}

TEST_CASE("GUI input: an integer slider stays on whole numbers")
{
    GuiFixture f;
    int32_t value = 0;
    const auto body = [&] { std::ignore = gui::slider_int("steps", value, 0, 10); };
    f.driver.settle(f.column_of(body));
    const Rect track = track_of(f, "steps");
    f.driver.move_to({ track.min[0] + track.size[0] * 0.33f, rect_centre(track)[1] });
    f.driver.frame(f.column_of(body));
    f.driver.press();
    f.driver.frame(f.column_of(body));
    CHECK(value == 3);
}

TEST_CASE("GUI input: dragging changes a value by speed per pixel and clamps")
{
    GuiFixture f;
    float amount = 0.0f;
    int32_t count = 0;
    const auto body = [&]
    {
        std::ignore = gui::drag_float("amount", amount, 0.0f, 12.0f, { .speed = 0.5f });
        std::ignore = gui::drag_int("count", count, 0, 100, { .speed = 0.1f });
    };
    f.driver.settle(f.column_of(body));
    const Vec2f from = rect_centre(track_of(f, "amount"));
    f.driver.drag(from, from + Vec2f(20.0f, 0.0f), 5, f.column_of(body));
    CHECK(amount == doctest::Approx(10.0f));
    f.driver.drag(from, from + Vec2f(60.0f, 0.0f), 5, f.column_of(body));
    CHECK(amount == doctest::Approx(12.0f));
    const Vec2f count_from = rect_centre(track_of(f, "count"));
    f.driver.drag(count_from, count_from + Vec2f(30.0f, 0.0f), 6, f.column_of(body));
    CHECK(count == 3);
}

TEST_CASE("GUI input: a combo opens, picks, and closes on a pick, an outside press or Escape")
{
    GuiFixture f;
    const std::string_view items[] = { "alpha", "beta", "gamma" };
    int32_t selected = 0;
    bool changed = false;
    f.driver.input().surface_size = { 200.0f, 200.0f };
    const auto body = [&] { changed = gui::combo("mode", items, selected) || changed; };
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("beta") == nullptr);
    f.driver.click(rect_centre(f.find_text("alpha")->rect), f.column_of(body));
    f.driver.settle(f.column_of(body));
    const LayoutNode* beta = f.find_text("beta");
    REQUIRE(beta != nullptr);
    f.driver.click(rect_centre(beta->rect), f.column_of(body));
    f.driver.settle(f.column_of(body));
    CHECK(selected == 1);
    CHECK(changed);
    CHECK(f.find_text("gamma") == nullptr);

    f.driver.click(rect_centre(f.find_text("beta")->rect), f.column_of(body));
    f.driver.settle(f.column_of(body));
    REQUIRE(f.find_text("gamma") != nullptr);
    f.driver.click({ 190.0f, 190.0f }, f.column_of(body));
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("gamma") == nullptr);
    CHECK(selected == 1);

    f.driver.click(rect_centre(f.find_text("beta")->rect), f.column_of(body));
    f.driver.settle(f.column_of(body));
    REQUIRE(f.find_text("gamma") != nullptr);
    f.driver.key_press(ImKey::Escape);
    f.driver.run_frames(2, f.column_of(body));
    CHECK(f.find_text("gamma") == nullptr);
}

TEST_CASE("GUI input: warm frames allocate nothing")
{
    GuiFixture f;
    float slider = 3.0f;
    float drag = 1.0f;
    int32_t selected = 0;
    const std::string_view items[] = { "alpha", "beta" };
    const auto body = [&]
    {
        std::ignore = gui::slider_float("slider", slider, 0.0f, 10.0f);
        std::ignore = gui::drag_float("drag", drag);
        std::ignore = gui::combo("mode", items, selected);
    };
    f.driver.run_frames(5, f.column_of(body));
    f.driver.click(rect_centre(f.find_text("alpha")->rect), f.column_of(body));
    f.driver.run_frames(4, f.column_of(body));
    const MemoryStats before = test::all_allocations();
    f.driver.run_frames(3, f.column_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
