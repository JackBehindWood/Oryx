#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Interface/Canvas/TabShape.h"

using namespace oryx;

namespace
{

const Rect k_tab{ { 10.0f, 4.0f }, { 80.0f, 20.0f } };

float cross(const Vec2f& a, const Vec2f& b, const Vec2f& c)
{
    return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0]);
}

bool convex(const Vec2f* points, uint32_t count)
{
    bool positive = false;
    bool negative = false;
    for (uint32_t index = 0; index < count; ++index)
    {
        const float turn = cross(points[index], points[(index + 1) % count], points[(index + 2) % count]);
        positive = positive || turn > 1.0e-3f;
        negative = negative || turn < -1.0e-3f;
    }
    return !(positive && negative);
}

} // namespace

TEST_CASE("tab_outline: without a slope the outline is the rect")
{
    Vec2f points[k_max_polygon_points];
    const uint32_t count = tab_outline(k_tab, 0.0f, 4.0f, false, points);
    REQUIRE(count == 4);
    CHECK(points[0] == Vec2f(10.0f, 24.0f));
    CHECK(points[1] == Vec2f(10.0f, 4.0f));
    CHECK(points[2] == Vec2f(90.0f, 4.0f));
    CHECK(points[3] == Vec2f(90.0f, 24.0f));
}

TEST_CASE("tab_outline: the base keeps the full width and the top is inset by the slope")
{
    Vec2f points[k_max_polygon_points];
    const uint32_t count = tab_outline(k_tab, 8.0f, 4.0f, false, points);
    REQUIRE(count >= 6);
    CHECK(count <= k_max_polygon_points);
    CHECK(points[0] == Vec2f(10.0f, 24.0f));
    CHECK(points[count - 1] == Vec2f(90.0f, 24.0f));
    float top_min = 1.0e9f;
    float top_max = -1.0e9f;
    for (uint32_t index = 0; index < count; ++index)
    {
        CHECK(points[index][1] >= 4.0f - 1.0e-3f);
        CHECK(points[index][1] <= 24.0f + 1.0e-3f);
        CHECK(points[index][0] >= 10.0f - 1.0e-3f);
        CHECK(points[index][0] <= 90.0f + 1.0e-3f);
        if (points[index][1] < 4.0f + 1.0e-3f)
        {
            top_min = math::min(top_min, points[index][0]);
            top_max = math::max(top_max, points[index][0]);
        }
    }
    CHECK(top_min >= 18.0f - 1.0e-3f);
    CHECK(top_max <= 82.0f + 1.0e-3f);
    CHECK(top_min < top_max);
}

TEST_CASE("tab_outline: the polygon is convex for any slope and shoulder")
{
    for (float slope : { 1.0f, 8.0f, 20.0f, 40.0f, 80.0f })
    {
        for (float shoulder : { 0.0f, 3.0f, 12.0f, 50.0f })
        {
            Vec2f points[k_max_polygon_points];
            const uint32_t count = tab_outline(k_tab, slope, shoulder, false, points);
            CHECK(count >= 4);
            CHECK(convex(points, count));
        }
    }
}

TEST_CASE("tab_outline: flip mirrors the tab onto the other edge")
{
    Vec2f up[k_max_polygon_points];
    Vec2f down[k_max_polygon_points];
    const uint32_t count = tab_outline(k_tab, 8.0f, 4.0f, false, up);
    REQUIRE(tab_outline(k_tab, 8.0f, 4.0f, true, down) == count);
    CHECK(down[0] == Vec2f(90.0f, 4.0f));
    CHECK(down[count - 1] == Vec2f(10.0f, 4.0f));
    for (uint32_t index = 0; index < count; ++index)
    {
        CHECK(down[count - 1 - index][0] == doctest::Approx(up[index][0]));
        CHECK(down[count - 1 - index][1] == doctest::Approx(28.0f - up[index][1]));
    }
    CHECK(convex(down, count));
}

TEST_CASE("tab_outline: a rect with no size yields the plain corners")
{
    Vec2f points[k_max_polygon_points];
    CHECK(tab_outline(Rect{ { 0.0f, 0.0f }, { 0.0f, 10.0f } }, 8.0f, 4.0f, false, points) == 4);
}

TEST_CASE("LayoutTree paint: a sloped box records a polygon and an open outline")
{
    LayoutTree tree;
    LayoutStyle style;
    style.width = fixed(80.0f);
    style.height = fixed(20.0f);
    const uint32_t index = tree.begin_box(make_im_id("tab"), style, "tab");
    tree.node(index).paint.has_fill = true;
    tree.node(index).paint.fill = { 1.0f, 0.0f, 0.0f, 1.0f };
    tree.node(index).paint.border_width = 1.0f;
    tree.node(index).paint.border = { 0.0f, 1.0f, 0.0f, 1.0f };
    tree.node(index).paint.slope = 8.0f;
    tree.node(index).paint.shoulder = 4.0f;
    tree.end_box();
    tree.solve(Rect{ { 0.0f, 0.0f }, { 100.0f, 40.0f } }, TextMeasure{}, 1);
    DrawList list;
    tree.paint(list, nullptr, 1.0f);
    CHECK(list.command_count(DrawKind::Polygon) == 1);
    CHECK(list.command_count(DrawKind::Rect) == 0);
    CHECK(list.command_count(DrawKind::Border) == 0);
    const uint32_t points = list.channel(0).polygons[0].count;
    CHECK(list.command_count(DrawKind::Line) == points - 1);
}
