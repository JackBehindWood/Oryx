#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "NullWindow.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr float k_nan = std::numeric_limits<float>::quiet_NaN();
constexpr float k_inf = std::numeric_limits<float>::infinity();

BoardScene scene_of(SharedPtr<const BoardLayout> layout)
{
    BoardView view;
    view.layout = std::move(layout);
    std::vector<SpaceId> unchanged;
    BoardScene scene;
    build_board_scene(view, FakePresenter(), MoveBuilder(), { unchanged }, scene);
    return scene;
}

SharedPtr<const BoardLayout2D> single_circle()
{
    BoardLayout2D builder;
    builder.add_space("dot", { 0.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Circle);
    return builder.finish();
}

SpaceId pick_at(const BoardScene& scene, const Vec2f& viewport, const Vec2f& cursor)
{
    BoardProjection2D layout = fit_board_2d(scene, viewport);
    return space_at(layout_as<BoardLayout2D>(scene.layout, "test"), cursor_to_board(layout, cursor));
}

} // namespace

TEST_CASE("space_at includes a square's edges and corners and the board's outer corners")
{
    SharedPtr<const BoardLayout2D> grid = make_grid_layout(3, 3, false);

    CHECK(space_at(*grid, { 1.0f, 1.0f }) == 4);
    CHECK(space_at(*grid, { 1.5f, 1.0f }) == 4);
    CHECK(space_at(*grid, { 1.5f + 0.001f, 1.0f }) == 5);
    CHECK(space_at(*grid, { 1.0f, 1.5f }) == 1);
    CHECK(space_at(*grid, { -0.5f, 2.5f }) == 0);
    CHECK(space_at(*grid, { 2.5f, -0.5f }) == 8);
    CHECK(space_at(*grid, { 2.5f + 0.001f, 1.0f }) == k_no_space);
    CHECK(space_at(*grid, { -0.5f - 0.001f, 1.0f }) == k_no_space);
}

TEST_CASE("space_at gives a point on a shared edge or corner to the lowest space id")
{
    SharedPtr<const BoardLayout2D> grid = make_grid_layout(3, 3, false);

    CHECK(space_at(*grid, { 0.5f, 2.0f }) == 0);
    CHECK(space_at(*grid, { 0.5f, 1.5f }) == 0);
    CHECK(space_at(*grid, { 1.5f, 0.5f }) == 4);
    for (uint32_t repeat = 0; repeat < 3; ++repeat)
    {
        CHECK(space_at(*grid, { 1.5f, 1.5f }) == 1);
    }
}

TEST_CASE("space_at keeps a circle to its radius and off its square corners")
{
    SharedPtr<const BoardLayout2D> circle = single_circle();

    CHECK(space_at(*circle, { 0.0f, 0.0f }) == 0);
    CHECK(space_at(*circle, { 0.5f, 0.0f }) == 0);
    CHECK(space_at(*circle, { 0.0f, -0.5f }) == 0);
    CHECK(space_at(*circle, { 0.5001f, 0.0f }) == k_no_space);
    CHECK(space_at(*circle, { 0.35f, 0.35f }) == 0);
    CHECK(space_at(*circle, { 0.36f, 0.36f }) == k_no_space);
    CHECK(space_at(*circle, { 0.5f, 0.5f }) == k_no_space);
}

TEST_CASE("space_at and cursor_to_board find nothing for a NaN or infinite cursor")
{
    BoardScene scene = scene_of(make_grid_layout(3, 3, false));
    Vec2f viewport = { 800.0f, 600.0f };
    BoardProjection2D layout = fit_board_2d(scene, viewport);
    REQUIRE(layout.scale > 0.0f);

    for (Vec2f cursor : { Vec2f(k_nan, 300.0f), Vec2f(400.0f, k_nan), Vec2f(k_nan, k_nan), Vec2f(k_inf, 300.0f), Vec2f(400.0f, -k_inf), Vec2f(k_inf, k_inf) })
    {
        CAPTURE(cursor[0]);
        CAPTURE(cursor[1]);
        CHECK_NOTHROW(cursor_to_board(layout, cursor));
        CHECK(pick_at(scene, viewport, cursor) == k_no_space);
    }
    CHECK(pick_at(scene, viewport, { 400.0f, 300.0f }) != k_no_space);
}

TEST_CASE("A degenerate viewport picks nothing and never divides by zero")
{
    BoardScene scene = scene_of(make_grid_layout(3, 3, false));

    for (Vec2f viewport : { Vec2f(1.0f, 1.0f), Vec2f(0.0f, 0.0f), Vec2f(-5.0f, 600.0f), Vec2f(k_nan, 600.0f), Vec2f(800.0f, k_inf) })
    {
        CAPTURE(viewport[0]);
        CAPTURE(viewport[1]);
        BoardProjection2D layout = fit_board_2d(scene, viewport);
        CHECK_FALSE(layout.scale > 0.0f);
        CHECK(pick_at(scene, viewport, { 0.0f, 0.0f }) == k_no_space);
        CHECK(pick_at(scene, viewport, { 0.5f, 0.5f }) == k_no_space);
    }
}

TEST_CASE("At an extreme aspect ratio each space is either unreachable for the whole window or picked under its centre")
{
    BoardScene scene = scene_of(make_grid_layout(3, 3, false));

    for (Vec2f viewport : { Vec2f(100000.0f, 1.0f), Vec2f(1.0f, 100000.0f), Vec2f(10000.0f, 200.0f), Vec2f(200.0f, 10000.0f) })
    {
        CAPTURE(viewport[0]);
        CAPTURE(viewport[1]);
        BoardProjection2D layout = fit_board_2d(scene, viewport);
        for (SpaceId space = 0; space < scene.layout->space_count(); ++space)
        {
            Vec2f world = board_to_world(layout, scene.layout->position(space));
            Vec2f cursor = { world[0], viewport[1] - world[1] };
            CHECK(pick_at(scene, viewport, cursor) == (layout.scale > 0.0f ? space : k_no_space));
        }
    }
}

TEST_CASE("Picking works in logical points at any display scale, never in framebuffer pixels")
{
    Application app({ 0, nullptr });
    NullWindow& window = static_cast<NullWindow&>(app.adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", 400, 300 })));
    BoardScene scene = scene_of(make_grid_layout(3, 3, false));

    auto logical_viewport = [&window]()
    {
        NativeWindowHandle handle = window.native_handle();
        return Vec2f(static_cast<float>(handle.width), static_cast<float>(handle.height));
    };

    Vec2f viewport = logical_viewport();
    BoardProjection2D layout = fit_board_2d(scene, viewport);
    std::vector<Vec2f> cursors;
    for (SpaceId space = 0; space < scene.layout->space_count(); ++space)
    {
        Vec2f world = board_to_world(layout, scene.layout->position(space));
        cursors.push_back({ world[0], viewport[1] - world[1] });
    }

    for (float scale : { 1.0f, 1.5f, 2.0f, 3.0f })
    {
        CAPTURE(scale);
        window.inject_scale(scale);
        REQUIRE(window.native_handle().framebuffer_width == static_cast<int32_t>(std::lround(400.0f * scale)));
        REQUIRE(logical_viewport() == viewport);

        for (SpaceId space = 0; space < cursors.size(); ++space)
        {
            window.inject_cursor(cursors[space][0], cursors[space][1]);
            BoardInput input = read_board_input(window.input(), logical_viewport());
            CHECK(input.viewport == viewport);
            CHECK(pick_at(scene, input.viewport, input.cursor) == space);
        }
    }

    window.inject_scale(2.0f);
    NativeWindowHandle handle = window.native_handle();
    Vec2f wrong = { static_cast<float>(handle.framebuffer_width), static_cast<float>(handle.framebuffer_height) };
    CHECK(pick_at(scene, wrong, cursors[4]) != 4);
}
