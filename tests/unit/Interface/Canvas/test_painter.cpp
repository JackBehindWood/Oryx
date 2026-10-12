#include "doctest.h"

#include "Oryx.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };

struct PainterFixture
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    DrawList list;
};

} // namespace

TEST_CASE("Painter: rects snap to device pixels")
{
    PainterFixture f;
    Painter painter(f.list, f.font, 2.0f);
    painter.fill_rect({ { 0.3f, 0.3f }, { 10.2f, 10.2f } }, RED);
    CHECK(f.list.channel(0).rects[0].rect == Rect{ { 0.5f, 0.5f }, { 10.0f, 10.0f } });
}

TEST_CASE("Painter: text is centred vertically on the ascent and placed by alignment")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    const Rect box = { { 10.0f, 20.0f }, { 100.0f, 30.0f } };
    painter.text(box, "AB", { .pixel_height = 16.0f, .align = TextAlign::Left });
    painter.text(box, "AB", { .pixel_height = 16.0f, .align = TextAlign::Centre });
    painter.text(box, "AB", { .pixel_height = 16.0f, .align = TextAlign::Right });
    const std::vector<TextCmd>& texts = f.list.channel(0).texts;
    REQUIRE(texts.size() == 3);
    CHECK(texts[0].origin == Vec2f(10.0f, 41.0f));
    CHECK(texts[1].origin == Vec2f(60.0f, 41.0f));
    CHECK(texts[2].origin == Vec2f(110.0f, 41.0f));
    CHECK(f.list.channel(0).texts[0].clip == k_no_clip);
}

TEST_CASE("Painter: overlong text without ellipsis is clipped to the box")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    const Rect box = { { 0.0f, 0.0f }, { 25.0f, 20.0f } };
    painter.text(box, "ABCDE", { .pixel_height = 16.0f });
    REQUIRE(f.list.channel(0).texts.size() == 1);
    CHECK(f.list.text(f.list.channel(0).texts[0]) == "ABCDE");
    CHECK(f.list.clip(f.list.channel(0).texts[0].clip) == box);
    CHECK_FALSE(f.list.has_clip());
}

TEST_CASE("Painter: ellipsis shortens text until it fits the box")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    const Rect box = { { 0.0f, 0.0f }, { 45.0f, 20.0f } };
    painter.text(box, "ABCDE", { .pixel_height = 16.0f, .ellipsis = true });
    REQUIRE(f.list.channel(0).texts.size() == 1);
    const std::string_view shown = f.list.text(f.list.channel(0).texts[0]);
    CHECK(shown.size() < 5 + 3);
    CHECK(shown.substr(shown.size() - 3) == "...");
    CHECK(painter.text_width(shown, 16.0f) <= box.size[0]);
    CHECK(f.list.channel(0).texts[0].clip == k_no_clip);
}

TEST_CASE("Painter: text that fits is recorded unchanged")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    painter.text({ { 0.0f, 0.0f }, { 200.0f, 20.0f } }, "ABC", { .ellipsis = true });
    CHECK(f.list.text(f.list.channel(0).texts[0]) == "ABC");
}

TEST_CASE("Painter: nothing is recorded while the font is not ready or the box is empty")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    painter.text({ { 0.0f, 0.0f }, { 0.0f, 20.0f } }, "AB");
    f.source->is_ready = false;
    painter.text({ { 0.0f, 0.0f }, { 100.0f, 20.0f } }, "AB");
    CHECK(f.list.command_count() == 0);
}

TEST_CASE("PainterClip pops on scope exit and on a throw")
{
    PainterFixture f;
    try
    {
        PainterClip clip(f.list, { { 0.0f, 0.0f }, { 5.0f, 5.0f } });
        CHECK(f.list.clip_depth() == 1);
        throw Error("boom");
    }
    catch (const Error&)
    {
    }
    CHECK(f.list.clip_depth() == 0);
}

TEST_CASE("Painter: convex polygons snap their points to device pixels")
{
    PainterFixture f;
    Painter painter(f.list, f.font, 2.0f);
    const Vec2f points[4] = { { 0.3f, 0.3f }, { 10.2f, 0.3f }, { 8.8f, 6.1f }, { 1.6f, 6.1f } };
    painter.fill_convex(points, 4, RED);
    const PolygonCmd& command = f.list.channel(0).polygons[0];
    REQUIRE(command.count == 4);
    CHECK(command.points[0] == Vec2f(0.5f, 0.5f));
    CHECK(command.points[1] == Vec2f(10.0f, 0.5f));
    CHECK(command.points[2] == Vec2f(9.0f, 6.0f));
}

TEST_CASE("Painter: polygons and polylines reject point counts out of range")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    const Vec2f points[k_max_polygon_points + 1] = {};
    CHECK_THROWS_AS(painter.fill_convex(points, 2, RED), Error);
    CHECK_THROWS_AS(painter.fill_convex(points, k_max_polygon_points + 1, RED), Error);
    CHECK_THROWS_AS(painter.stroke_polyline(points, 1, false, 1.0f, RED), Error);
}

TEST_CASE("Painter: stroke_polyline draws one line per segment and closes on request")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    const Vec2f points[3] = { { 0.0f, 0.0f }, { 10.0f, 0.0f }, { 10.0f, 10.0f } };
    painter.stroke_polyline(points, 3, false, 1.0f, RED);
    CHECK(f.list.channel(0).lines.size() == 2);
    painter.stroke_polyline(points, 3, true, 1.0f, RED);
    CHECK(f.list.channel(0).lines.size() == 5);
}

TEST_CASE("Painter: circles are fully rounded squares")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    painter.fill_circle({ 10.0f, 10.0f }, 4.0f, RED);
    painter.stroke_circle({ 10.0f, 10.0f }, 4.0f, 1.0f, RED);
    REQUIRE(f.list.channel(0).rounded_rects.size() == 1);
    CHECK(f.list.channel(0).rounded_rects[0].rect == Rect{ { 6.0f, 6.0f }, { 8.0f, 8.0f } });
    CHECK(f.list.channel(0).rounded_rects[0].radius.top_left == 4.0f);
    CHECK(f.list.channel(0).borders.size() == 1);
}

TEST_CASE("Painter: triangles record a three point polygon")
{
    PainterFixture f;
    Painter painter(f.list, f.font);
    painter.fill_triangle({ 0.0f, 0.0f }, { 8.0f, 0.0f }, { 4.0f, 6.0f }, RED);
    CHECK(f.list.channel(0).polygons[0].count == 3);
}
