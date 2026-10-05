#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "NullRHI.h"
#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

constexpr Vec2f kViewport = { 800.0f, 600.0f };

BoardInput hover(const PresentedGraphicsBoard& board, SpaceId space)
{
    BoardScene scene;
    board.presentation().build_scene(kNoSpace, scene);
    BoardLayout2D layout = fit_board_2d(scene, kViewport);
    const Vec3f& position = scene.spaces[space].space.position;
    Vec2f world = board_to_world(layout, { position[0], position[1] });

    BoardInput input;
    input.viewport = kViewport;
    input.cursor = { world[0], kViewport[1] - world[1] };
    return input;
}

BoardInput click(const PresentedGraphicsBoard& board, SpaceId space)
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

TEST_CASE("PresentedGraphicsBoard turns a click on a cell into its move, once")
{
    PresentedGraphicsBoard board(create_unique<TicTacToePresenter>(), "tictactoe", kAllSeats);
    TicTacToeState state;
    board.on_turn(state);

    CHECK(board.poll_action(state) == PENDING_ACTION);
    board.update(click(board, 4), 0.016);
    CHECK(board.poll_action(state) == 4);
    CHECK(board.poll_action(state) == PENDING_ACTION);
    CHECK(board.shows_moves());
}

TEST_CASE("PresentedGraphicsBoard builds a two-click move and tracks the hovered space")
{
    PresentedGraphicsBoard board(create_unique<HexapawnPresenter>(), "hexapawn", kAllSeats);
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

TEST_CASE("PresentedGraphicsBoard takes back a pick, drops it on a click that fits no move, and queues undo")
{
    PresentedGraphicsBoard board(create_unique<HexapawnPresenter>(), "hexapawn", kAllSeats);
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

TEST_CASE("PresentedGraphicsBoard ignores clicks on another seat's turn and forgets a queued move when the state changes")
{
    HexapawnState state;
    PresentedGraphicsBoard black(create_unique<HexapawnPresenter>(), "hexapawn", 1);
    black.on_turn(state);
    black.update(click(black, 6), 0.016);
    CHECK(black.presentation().builder().picked().empty());

    PresentedGraphicsBoard board(create_unique<TicTacToePresenter>(), "tictactoe", kAllSeats);
    TicTacToeState tic_tac_toe;
    board.on_turn(tic_tac_toe);
    board.update(click(board, 4), 0.016);
    tic_tac_toe.apply(0);
    board.on_turn(tic_tac_toe);
    CHECK(board.poll_action(tic_tac_toe) == PENDING_ACTION);
}

TEST_CASE("PresentedGraphicsBoard picks menu options with the buttons")
{
    PresentedGraphicsBoard board(create_unique<FakePresenter>(), "dummy", kAllSeats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);

    BoardScene scene;
    board.presentation().build_scene(kNoSpace, scene);
    BoardLayout2D layout = fit_board_2d(scene, kViewport);
    std::vector<OptionButton2D> buttons;
    option_buttons_2d(layout, scene.options.size(), buttons);
    REQUIRE(buttons.size() == 3);

    BoardInput input;
    input.viewport = kViewport;
    input.cursor = { buttons[1].centre[0], kViewport[1] - buttons[1].centre[1] };
    input.select = true;
    board.update(input, 0.016);
    CHECK(board.poll_action(*state) == 2);
}

TEST_CASE("PresentedGraphicsBoard draws the scene through the renderer, and nothing into a degenerate window")
{
    RendererGuard renderer;
    PresentedGraphicsBoard board(create_unique<HexapawnPresenter>(), "hexapawn", kAllSeats);
    HexapawnState state;
    board.on_turn(state);
    board.update(click(board, 6), 0.016);

    board.render(hover(board, 3));
    CHECK(Renderer::batch_stats().primitives > 9);

    uint32_t before = Renderer::batch_stats().primitives;
    BoardInput tiny;
    tiny.viewport = { 4.0f, 4.0f };
    board.render(tiny);
    CHECK(Renderer::batch_stats().primitives == before);
}
