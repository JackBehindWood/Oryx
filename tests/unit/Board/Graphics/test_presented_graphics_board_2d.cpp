#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"
#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

constexpr Vec2f k_viewport = { 800.0f, 600.0f };

BoardInput hover(const PresentedGraphicsBoard2D& board, SpaceId space)
{
    BoardScene scene;
    board.presentation().build_scene(k_no_space, scene);
    BoardProjection2D layout = fit_board_2d(scene, k_viewport);
    Vec2f world = board_to_world(layout, scene.layout->position(space));

    BoardInput input;
    input.viewport = k_viewport;
    input.cursor = { world[0], k_viewport[1] - world[1] };
    return input;
}

BoardInput click(const PresentedGraphicsBoard2D& board, SpaceId space)
{
    BoardInput input = hover(board, space);
    input.select = true;
    input.restart = true;
    return input;
}

ActionId pawn(uint32_t from, HexapawnState::Direction direction)
{
    return HexapawnState::action_for(from, direction);
}

struct RendererGuard
{
    RendererGuard() { Renderer::init({ RHIBackend::Null }); }
    ~RendererGuard() { Renderer::shutdown(); }
};

} // namespace

TEST_CASE("PresentedGraphicsBoard2D turns a click on a cell into its move, once")
{
    PresentedGraphicsBoard2D board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
    TicTacToeState state;
    board.on_turn(state);

    CHECK(board.poll_action(state) == PENDING_ACTION);
    board.update(click(board, 4), 0.016);
    CHECK(board.poll_action(state) == 4);
    CHECK(board.poll_action(state) == PENDING_ACTION);
    CHECK(board.shows_moves());
}

TEST_CASE("PresentedGraphicsBoard2D builds a two-click move and tracks the hovered space")
{
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);

    board.update(hover(board, 6), 0.016);
    CHECK(board.hovered() == 6);

    board.update(click(board, 6), 0.016);
    CHECK(board.poll_action(state) == PENDING_ACTION);
    CHECK(board.presentation().builder().picked().size() == 1);
    board.update(click(board, 3), 0.016);
    CHECK(board.poll_action(state) == pawn(6, HexapawnState::Forward));
}

TEST_CASE("PresentedGraphicsBoard2D takes back a pick, drops it on a click that fits no move, and queues undo")
{
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);

    board.update(click(board, 6), 0.016);
    BoardInput back = hover(board, 6);
    back.back = true;
    board.update(back, 0.016);
    CHECK(board.presentation().builder().picked().empty());

    board.update(click(board, 6), 0.016);
    board.update(click(board, 4), 0.016);
    CHECK(board.presentation().builder().picked().empty());
    CHECK(board.poll_action(state) == PENDING_ACTION);

    BoardInput undo = hover(board, 0);
    undo.undo = true;
    board.update(undo, 0.016);
    CHECK(board.poll_action(state) == UNDO_ACTION);
}

TEST_CASE("PresentedGraphicsBoard2D ignores clicks on another seat's turn and forgets a queued move when the state changes")
{
    HexapawnState state;
    PresentedGraphicsBoard2D black(create_unique<HexapawnPresenter>(), "hexapawn", 1);
    black.on_turn(state);
    black.update(click(black, 6), 0.016);
    CHECK(black.presentation().builder().picked().empty());

    PresentedGraphicsBoard2D board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
    TicTacToeState tic_tac_toe;
    board.on_turn(tic_tac_toe);
    board.update(click(board, 4), 0.016);
    tic_tac_toe.apply(0);
    board.on_turn(tic_tac_toe);
    CHECK(board.poll_action(tic_tac_toe) == PENDING_ACTION);
}

TEST_CASE("PresentedGraphicsBoard2D picks menu options with the buttons")
{
    PresentedGraphicsBoard2D board(create_unique<FakePresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);

    BoardScene scene;
    board.presentation().build_scene(k_no_space, scene);
    BoardProjection2D layout = fit_board_2d(scene, k_viewport);
    std::vector<OptionButton2D> buttons;
    option_buttons_2d(layout, scene.options.size(), buttons);
    REQUIRE(buttons.size() == 3);

    BoardInput input;
    input.viewport = k_viewport;
    input.cursor = { buttons[1].centre[0], k_viewport[1] - buttons[1].centre[1] };
    input.select = true;
    board.update(input, 0.016);
    CHECK(board.poll_action(*state) == 2);
}

TEST_CASE("PresentedGraphicsBoard2D draws the scene through the renderer, and nothing into a degenerate window")
{
    RendererGuard renderer;
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);
    board.update(click(board, 6), 0.016);

    render_board(board, hover(board, 3));
    CHECK(Renderer::batch_stats().primitives > 9);

    uint32_t before = Renderer::batch_stats().primitives;
    BoardInput tiny;
    tiny.viewport = { 4.0f, 4.0f };
    render_board(board, tiny);
    CHECK(Renderer::batch_stats().primitives == before);
}

namespace
{

BoardInput frame_at(const PresentedGraphicsBoard2D& board, SpaceId space, bool select, bool down, bool released)
{
    BoardInput input = hover(board, space);
    input.select = select;
    input.select_down = down;
    input.select_released = released;
    return input;
}

} // namespace

TEST_CASE("PresentedGraphicsBoard2D plays a move by dragging a piece onto its target")
{
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);

    board.update(frame_at(board, 6, true, true, false), 0.016);
    CHECK(board.dragging());
    board.update(frame_at(board, 4, false, true, false), 0.016);
    CHECK(board.dragging());
    CHECK(board.poll_action(state) == PENDING_ACTION);

    BoardScene scene;
    board.presentation().build_scene(k_no_space, scene);
    board.update(frame_at(board, 3, false, false, true), 0.016);
    CHECK_FALSE(board.dragging());
    CHECK(board.poll_action(state) == pawn(6, HexapawnState::Forward));
}

TEST_CASE("PresentedGraphicsBoard2D treats a release over the pressed cell as a click and a drop elsewhere as nothing")
{
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);

    board.update(frame_at(board, 6, true, true, false), 0.016);
    board.update(frame_at(board, 6, false, false, true), 0.016);
    CHECK_FALSE(board.dragging());
    CHECK(board.presentation().builder().picked().size() == 1);

    board.update(frame_at(board, 7, true, true, false), 0.016);
    board.update(frame_at(board, 8, false, false, true), 0.016);
    CHECK(board.presentation().builder().picked().size() == 1);
    CHECK(board.poll_action(state) == PENDING_ACTION);
}

TEST_CASE("PresentedGraphicsBoard2D cancels a drag on back and ends one whose release it never saw")
{
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);

    board.update(frame_at(board, 6, true, true, false), 0.016);
    BoardInput cancel = frame_at(board, 3, false, true, false);
    cancel.back = true;
    board.update(cancel, 0.016);
    CHECK_FALSE(board.dragging());
    CHECK(board.presentation().builder().picked().empty());

    board.update(frame_at(board, 6, true, true, false), 0.016);
    board.update(frame_at(board, 3, false, false, false), 0.016);
    CHECK_FALSE(board.dragging());
    CHECK(board.presentation().builder().picked().size() == 1);
}
