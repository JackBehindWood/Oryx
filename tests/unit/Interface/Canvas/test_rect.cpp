#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("Rect: contains is half-open")
{
    const Rect rect = { { 10.0f, 20.0f }, { 30.0f, 40.0f } };
    CHECK(contains(rect, { 10.0f, 20.0f }));
    CHECK(contains(rect, { 39.9f, 59.9f }));
    CHECK_FALSE(contains(rect, { 40.0f, 30.0f }));
    CHECK_FALSE(contains(rect, { 20.0f, 60.0f }));
    CHECK_FALSE(contains(rect, { 9.9f, 30.0f }));
}

TEST_CASE("Rect: intersect clamps to an empty rect when apart")
{
    const Rect a = { { 0.0f, 0.0f }, { 10.0f, 10.0f } };
    const Rect b = { { 5.0f, 6.0f }, { 10.0f, 10.0f } };
    const Rect overlap = intersect(a, b);
    CHECK(overlap == Rect{ { 5.0f, 6.0f }, { 5.0f, 4.0f } });
    CHECK(overlaps(a, b));
    const Rect far = { { 20.0f, 20.0f }, { 5.0f, 5.0f } };
    CHECK(is_empty(intersect(a, far)));
    CHECK_FALSE(overlaps(a, far));
    CHECK_FALSE(overlaps(a, Rect{ { 10.0f, 0.0f }, { 5.0f, 5.0f } }));
}

TEST_CASE("Rect: inset and expand")
{
    const Rect rect = { { 10.0f, 10.0f }, { 20.0f, 10.0f } };
    CHECK(inset(rect, { 1.0f, 2.0f, 3.0f, 4.0f }) == Rect{ { 11.0f, 12.0f }, { 16.0f, 4.0f } });
    CHECK(is_empty(inset(rect, uniform_insets(20.0f))));
    CHECK(inset(rect, uniform_insets(20.0f)).size[0] == 0.0f);
    CHECK(expand(rect, uniform_insets(2.0f)) == Rect{ { 8.0f, 8.0f }, { 24.0f, 14.0f } });
    CHECK(rect_centre(rect) == Vec2f(20.0f, 15.0f));
    CHECK(rect_max(rect) == Vec2f(30.0f, 20.0f));
}

TEST_CASE("Rect: at_least grows around the centre only when smaller")
{
    const Rect small = { { 10.0f, 10.0f }, { 10.0f, 10.0f } };
    CHECK(at_least(small, { 30.0f, 8.0f }) == Rect{ { 0.0f, 10.0f }, { 30.0f, 10.0f } });
    CHECK(at_least(small, { 4.0f, 4.0f }) == small);
}

TEST_CASE("Rect: radius clamps so neighbouring corners never overlap")
{
    const CornerRadius clamped = clamp_radius(uniform_radius(50.0f), { 40.0f, 20.0f });
    CHECK(clamped.top_left == doctest::Approx(10.0f));
    CHECK(clamped.bottom_right == doctest::Approx(10.0f));
    CHECK(clamp_radius(uniform_radius(-3.0f), { 40.0f, 20.0f }) == CornerRadius{});
    CHECK(is_square(CornerRadius{}));
    CHECK_FALSE(is_square(uniform_radius(2.0f)));
}

TEST_CASE("Rect: pixel snapping at a device scale")
{
    CHECK(snap_to_pixel(10.3f, 1.0f) == 10.0f);
    CHECK(snap_to_pixel(10.3f, 2.0f) == 10.5f);
    CHECK(snap_to_pixel(10.3f, 0.0f) == 10.3f);
    const Rect snapped = snap_to_pixels({ { 0.4f, 0.4f }, { 10.2f, 10.2f } }, 1.0f);
    CHECK(snapped == Rect{ { 0.0f, 0.0f }, { 11.0f, 11.0f } });
}
