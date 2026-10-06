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

TEST_CASE("option buttons sit centred in the menu band and are hit by the cursor")
{
    BoardProjection2D layout = fit_board_2d(grid_scene(3, 3), { 800.0f, 600.0f });
    std::vector<OptionButton2D> buttons;
    option_buttons_2d(layout, 3, buttons);
    REQUIRE(buttons.size() == 3);
    CHECK(buttons[1].centre[0] == doctest::Approx(400.0f));
    CHECK(buttons[0].centre[1] < k_board_menu_band);

    Vec2f over_last = { buttons[2].centre[0], layout.viewport[1] - buttons[2].centre[1] };
    CHECK(option_at(buttons, layout, over_last) == 2);
    CHECK(option_at(buttons, layout, { 400.0f, 10.0f }) == buttons.size());
}
