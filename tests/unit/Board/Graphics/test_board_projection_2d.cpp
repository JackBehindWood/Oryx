#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

BoardScene grid_scene(uint32_t columns, uint32_t rows)
{
    BoardView view;
    view.layout = make_grid_layout(columns, rows, false);
    std::vector<SpaceId> unchanged;
    BoardScene scene;
    build_board_scene(view, FakePresenter(), MoveBuilder(), { unchanged }, scene);
    return scene;
}

// The cursor (y down from the top) over a board point.
Vec2f cursor_at(const BoardProjection2D& layout, const Vec2f& board)
{
    Vec2f world = board_to_world(layout, board);
    return { world[0], layout.viewport[1] - world[1] };
}

} // namespace

TEST_CASE("fit_board_2d centres the board between the status and menu bands at any window size")
{
    BoardScene scene = grid_scene(3, 3);
    for (Vec2f viewport : { Vec2f(800.0f, 600.0f), Vec2f(1600.0f, 1200.0f), Vec2f(500.0f, 900.0f) })
    {
        CAPTURE(viewport[0]);
        BoardProjection2D layout = fit_board_2d(scene, viewport);
        REQUIRE(layout.scale > 0.0f);

        Vec2f low = board_to_world(layout, scene.layout->min());
        Vec2f high = board_to_world(layout, scene.layout->max());
        CHECK(low[0] >= k_board_gutter - 0.01f);
        CHECK(high[0] <= viewport[0] - k_board_gutter + 0.01f);
        CHECK(low[1] >= k_board_menu_band + k_board_gutter - 0.01f);
        CHECK(high[1] <= viewport[1] - k_board_status_band + 0.01f);
        CHECK((low[0] + high[0]) * 0.5f == doctest::Approx(viewport[0] * 0.5f));
    }
}

TEST_CASE("cursor_to_board inverts the layout so every space is hit under its centre")
{
    SharedPtr<const BoardLayout2D> board = make_grid_layout(8, 8, true);
    BoardScene scene = grid_scene(8, 8);
    BoardProjection2D layout = fit_board_2d(scene, { 1280.0f, 720.0f });

    for (SpaceId space = 0; space < board->space_count(); ++space)
    {
        Vec2f cursor = cursor_at(layout, board->position(space));
        CHECK(space_at(*board, cursor_to_board(layout, cursor)) == space);
    }
    CHECK(space_at(*board, cursor_to_board(layout, { 1.0f, 1.0f })) == k_no_space);
}

TEST_CASE("fit_board_2d gives a zero scale for a degenerate window or board, and nothing is hit")
{
    BoardProjection2D layout = fit_board_2d(grid_scene(3, 3), { 10.0f, 10.0f });
    CHECK(layout.scale == 0.0f);
    CHECK(space_at(*make_grid_layout(3, 3, false), cursor_to_board(layout, { 5.0f, 5.0f })) == k_no_space);

    CHECK(fit_board_2d(BoardScene{}, { 800.0f, 600.0f }).scale == 0.0f);
}

TEST_CASE("fit_board_2d inside a region keeps the board within it, and the viewport form uses the band region")
{
    BoardScene scene = grid_scene(3, 3);
    const Vec2f viewport = { 800.0f, 600.0f };
    Rect region = { { 100.0f, 80.0f }, { 300.0f, 200.0f } };
    BoardProjection2D layout = fit_board_2d(scene, viewport, region);
    REQUIRE(layout.scale > 0.0f);

    Vec2f low = board_to_world(layout, scene.layout->min());
    Vec2f high = board_to_world(layout, scene.layout->max());
    CHECK(low[0] >= region.min[0] - 0.01f);
    CHECK(high[0] <= region.min[0] + region.size[0] + 0.01f);
    CHECK(low[1] >= viewport[1] - region.min[1] - region.size[1] - 0.01f);
    CHECK(high[1] <= viewport[1] - region.min[1] + 0.01f);

    BoardProjection2D bands = fit_board_2d(scene, viewport);
    BoardProjection2D explicit_region = fit_board_2d(scene, viewport, board_region_2d(viewport));
    CHECK(bands.scale == explicit_region.scale);
    CHECK(bands.origin == explicit_region.origin);
    CHECK(fit_board_2d(scene, viewport, { { 0.0f, 0.0f }, { 0.0f, 50.0f } }).scale == 0.0f);
}

TEST_CASE("board_region_2d leaves the status band on top and the menu band and gutters elsewhere")
{
    Rect region = board_region_2d({ 800.0f, 600.0f });
    CHECK(region.min == Vec2f(k_board_gutter, k_board_status_band));
    CHECK(rect_max(region)[0] == doctest::Approx(800.0f - k_board_gutter));
    CHECK(rect_max(region)[1] == doctest::Approx(600.0f - k_board_menu_band - k_board_gutter));
}
