#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"
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

namespace
{

BoardInput over_button(const PresentedGraphicsBoard2D& board, size_t index, const Vec2f& viewport = k_viewport)
{
    BoardInput input;
    input.viewport = viewport;
    input.cursor = rect_centre(board.overlay().buttons[index].rect);
    return input;
}

BoardInput idle()
{
    BoardInput input;
    input.viewport = k_viewport;
    input.cursor = { -1.0f, -1.0f };
    return input;
}

}

TEST_CASE("PresentedGraphicsBoard2D picks a menu option when its button is released, not when it is pressed")
{
    PresentedGraphicsBoard2D board(create_unique<FakePresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);
    board.update(idle(), 0.016);
    REQUIRE(board.overlay().buttons.size() == 3);

    BoardInput press = over_button(board, 1);
    press.select = true;
    press.select_down = true;
    board.update(press, 0.016);
    CHECK(board.poll_action(*state) == PENDING_ACTION);

    BoardInput release = over_button(board, 1);
    release.select_released = true;
    board.update(release, 0.016);
    CHECK(board.poll_action(*state) == 2);
}

TEST_CASE("PresentedGraphicsBoard2D lays the buttons in a centred row under the board")
{
    PresentedGraphicsBoard2D board(create_unique<FakePresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);

    for (const Vec2f& viewport : { Vec2f(800.0f, 600.0f), Vec2f(1280.0f, 720.0f), Vec2f(320.0f, 400.0f) })
    {
        BoardInput input = idle();
        input.viewport = viewport;
        board.update(input, 0.016);
        const BoardOverlayResult& overlay = board.overlay();
        REQUIRE(overlay.buttons.size() == 3);
        CHECK(overlay.board == board_region_2d(viewport));
        CHECK(rect_centre(overlay.buttons[1].rect)[0] == doctest::Approx(viewport[0] * 0.5f));
        for (size_t index = 0; index < 3; ++index)
        {
            const Rect& rect = overlay.buttons[index].rect;
            CHECK(rect.size == Vec2f(120.0f, 36.0f));
            CHECK(rect_centre(rect)[1] == doctest::Approx(viewport[1] - k_board_menu_band * 0.5f));
            CHECK_FALSE(overlaps(rect, overlay.board));
            if (index > 0)
            {
                CHECK(rect.min[0] - rect_max(overlay.buttons[index - 1].rect)[0] == doctest::Approx(12.0f));
            }
        }
    }
}

TEST_CASE("PresentedGraphicsBoard2D leaves a press on a button to the UI and a press on the board to the board")
{
    PresentedGraphicsBoard2D board(create_unique<FakePresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);
    board.update(idle(), 0.016);

    BoardInput press = over_button(board, 0);
    press.select = true;
    press.select_down = true;
    board.update(press, 0.016);
    CHECK(board.overlay().pointer_over_ui);
    CHECK_FALSE(board.dragging());

    BoardInput released_elsewhere = idle();
    released_elsewhere.select_released = true;
    board.update(released_elsewhere, 0.016);
    CHECK(board.poll_action(*state) == PENDING_ACTION);

    board.update(hover(board, 0), 0.016);
    CHECK_FALSE(board.overlay().pointer_over_ui);
}

TEST_CASE("PresentedGraphicsBoard2D does not choose an option from a press that began on the board")
{
    PresentedGraphicsBoard2D board(create_unique<FakePresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);
    board.update(idle(), 0.016);

    BoardInput press = hover(board, 0);
    press.select = true;
    press.select_down = true;
    board.update(press, 0.016);

    BoardInput release = over_button(board, 1);
    release.select_released = true;
    board.update(release, 0.016);
    CHECK(board.poll_action(*state) == PENDING_ACTION);
}

TEST_CASE("PresentedGraphicsBoard2D re-lays out the same frame at a new size or scale")
{
    PresentedGraphicsBoard2D board(create_unique<FakePresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);

    board.update(idle(), 0.016);
    CHECK(board.overlay().board == board_region_2d(k_viewport));

    BoardInput resized = idle();
    resized.viewport = { 1000.0f, 500.0f };
    resized.scale = 2.0f;
    board.update(resized, 0.016);
    CHECK(board.overlay().board == board_region_2d(resized.viewport));
    CHECK(board.ui().input().scale == 2.0f);
}

TEST_CASE("board_input_to_im maps a click, hides the pointer while it is captured or outside the window")
{
    BoardInput input;
    input.viewport = k_viewport;
    input.cursor = { 10.0f, 20.0f };
    input.select = true;
    input.scale = 2.0f;

    ImInput mapped = board_input_to_im(input, 0.5, false);
    const ImButton& left = button_of(mapped, MouseCode::Left);
    CHECK(left.pressed);
    CHECK(left.down);
    CHECK_FALSE(left.released);
    CHECK(mapped.pointer.valid);
    CHECK(mapped.pointer.position == Vec2f(10.0f, 20.0f));
    CHECK(mapped.surface_size == k_viewport);
    CHECK(mapped.scale == 2.0f);
    CHECK(mapped.delta_time == doctest::Approx(0.5f));

    CHECK_FALSE(board_input_to_im(input, 0.0, true).pointer.valid);
    input.cursor = { 801.0f, 20.0f };
    CHECK_FALSE(board_input_to_im(input, 0.0, false).pointer.valid);
}

namespace
{

struct FontTheme
{
    FakeFontSource* source = nullptr;
    Font font = make_fake_font(source);
    BoardTheme2D theme;

    FontTheme() { theme.font = &font; }
};

std::string overlay_text(const PresentedGraphicsBoard2D& board)
{
    return dump(board.ui().draw_list());
}

void play_x_wins(TicTacToeState& state)
{
    for (ActionId action : { 0, 3, 1, 4, 2 })
    {
        state.apply(action);
    }
}

}

TEST_CASE("PresentedGraphicsBoard2D says whose turn it is: a hot-seat board names the mark, a seated one says you or the opponent")
{
    FontTheme fonts;
    TicTacToeState state;

    PresentedGraphicsBoard2D hot_seat(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats, fonts.theme);
    hot_seat.on_turn(state);
    hot_seat.update(idle(), 0.016);
    CHECK(overlay_text(hot_seat).find("X to move") != std::string::npos);
    CHECK(overlay_text(hot_seat).find("Your turn") == std::string::npos);

    PresentedGraphicsBoard2D first(create_unique<TicTacToePresenter>(), "tictactoe", 0, fonts.theme);
    first.on_turn(state);
    first.update(idle(), 0.016);
    CHECK(overlay_text(first).find("Your turn - X to move") != std::string::npos);

    PresentedGraphicsBoard2D second(create_unique<TicTacToePresenter>(), "tictactoe", 1, fonts.theme);
    second.on_turn(state);
    second.update(idle(), 0.016);
    CHECK(overlay_text(second).find("Opponent's turn - X to move") != std::string::npos);

    state.apply(4);
    first.on_turn(state);
    first.update(idle(), 0.016);
    CHECK(overlay_text(first).find("Opponent's turn - O to move") != std::string::npos);
}

TEST_CASE("PresentedGraphicsBoard2D shows a result banner when the game ends: you win, you lose, or the winner by name")
{
    FontTheme fonts;
    TicTacToeState state;
    play_x_wins(state);

    PresentedGraphicsBoard2D winner(create_unique<TicTacToePresenter>(), "tictactoe", 0, fonts.theme);
    winner.on_turn(state);
    winner.update(idle(), 0.016);
    CHECK(overlay_text(winner).find("You win!") != std::string::npos);
    CHECK(overlay_text(winner).find("Your turn") == std::string::npos);
    CHECK(overlay_text(winner).find(k_restart_hint) != std::string::npos);

    PresentedGraphicsBoard2D loser(create_unique<TicTacToePresenter>(), "tictactoe", 1, fonts.theme);
    loser.on_turn(state);
    loser.update(idle(), 0.016);
    CHECK(overlay_text(loser).find("You lose") != std::string::npos);

    PresentedGraphicsBoard2D hot_seat(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats, fonts.theme);
    hot_seat.on_turn(state);
    hot_seat.update(idle(), 0.016);
    CHECK(overlay_text(hot_seat).find("X wins") != std::string::npos);

    TicTacToeState drawn;
    for (ActionId action : { 0, 1, 2, 4, 3, 5, 7, 6, 8 })
    {
        drawn.apply(action);
    }
    PresentedGraphicsBoard2D draw(create_unique<TicTacToePresenter>(), "tictactoe", 0, fonts.theme);
    draw.on_turn(drawn);
    draw.update(idle(), 0.016);
    CHECK(overlay_text(draw).find("Draw") != std::string::npos);
}

TEST_CASE("PresentedGraphicsBoard2D asks for a restart only from its Play again button, not from a click on the board")
{
    FontTheme fonts;
    PresentedGraphicsBoard2D board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats, fonts.theme);
    TicTacToeState state;
    play_x_wins(state);
    board.on_turn(state);
    board.update(idle(), 0.016);
    CHECK_FALSE(board.take_restart_request());

    board.update(click(board, 4), 0.016);
    CHECK_FALSE(board.take_restart_request());
    board.update(idle(), 0.016);

    Rect again;
    REQUIRE(board.ui().layout_rect(make_im_id("Play again", board.ui().id("result")), again));
    BoardInput press = idle();
    press.cursor = rect_centre(again);
    press.select = true;
    press.select_down = true;
    board.update(press, 0.016);
    CHECK_FALSE(board.take_restart_request());
    BoardInput release = press;
    release.select = false;
    release.select_down = false;
    release.select_released = true;
    board.update(release, 0.016);
    CHECK(board.take_restart_request());
    CHECK_FALSE(board.take_restart_request());
}

TEST_CASE("PresentedGraphicsBoard2D forgets the last game on reset: no highlight, a new seat, no waiting move or drag")
{
    PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    HexapawnState state;
    board.on_turn(state);
    board.update(click(board, 6), 0.016);
    board.update(click(board, 3), 0.016);
    REQUIRE(board.poll_action(state) == pawn(6, HexapawnState::Forward));
    state.apply(pawn(6, HexapawnState::Forward));
    board.on_turn(state);
    REQUIRE_FALSE(board.presentation().changed().empty());

    board.reset(1);
    HexapawnState fresh;
    board.on_turn(fresh);
    CHECK(board.presentation().changed().empty());
    CHECK(board.presentation().seat() == 1);
    CHECK(board.poll_action(fresh) == PENDING_ACTION);
    CHECK_FALSE(board.dragging());
    CHECK(board.hovered() == k_no_space);
    BoardScene scene;
    board.presentation().build_scene(k_no_space, scene);
    for (SpaceHighlight highlight : scene.highlights)
    {
        CHECK_FALSE(has_highlight(highlight, SpaceHighlight::Changed));
    }
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
