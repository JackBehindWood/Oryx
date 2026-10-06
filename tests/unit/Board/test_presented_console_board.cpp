#include "doctest.h"

#include "BoardTestSupport.h"

#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

PresentedConsoleBoard hexapawn_board()
{
    return PresentedConsoleBoard(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
}

ActionId pawn(uint32_t from, HexapawnState::Direction direction)
{
    return HexapawnState::action_for(from, direction);
}

} // namespace

TEST_CASE("board_text draws a grid with axis labels, the last move in brackets and the status")
{
    BoardPresentation presentation(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
    TicTacToeState state;
    presentation.update(state);
    state.apply(4);
    presentation.update(state);

    BoardScene scene;
    presentation.build_scene(k_no_space, scene);
    CHECK(board_text(scene) ==
          "3  .  .  .\n"
          "2  . [X] .\n"
          "1  .  .  .\n"
          "   a  b  c\n"
          "O to move\n");
}

TEST_CASE("board_text marks picked spaces in parentheses")
{
    BoardPresentation presentation(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
    presentation.update(HexapawnState());
    CHECK(presentation.builder().pick({ PickKind::Space, 6, {} }).status == PickStatus::Pending);

    BoardScene scene;
    presentation.build_scene(k_no_space, scene);
    CHECK(board_text(scene) ==
          "3  B  B  B\n"
          "2  .  .  .\n"
          "1 (W) W  W\n"
          "   a  b  c\n"
          "White to move\n");
}

TEST_CASE("PresentedConsoleBoard prints the board once per change and reads a move by its label")
{
    ConsoleScope console("B2\n");
    PresentedConsoleBoard board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
    TicTacToeState state;

    board.on_turn(state);
    board.on_turn(state);
    CHECK(board.poll_action(state) == 4);
    CHECK(board.shows_moves());

    std::string output = console.output();
    CHECK(output.find("X to move") == output.rfind("X to move"));
    CHECK(output.find("Moves: a3 b3 c3 a2 b2 c2 a1 b1 c1") != std::string::npos);
}

TEST_CASE("PresentedConsoleBoard builds a multi-pick move on one line or across lines")
{
    HexapawnState state;
    {
        ConsoleScope console("a1 a2\n");
        PresentedConsoleBoard board = hexapawn_board();
        CHECK(board.poll_action(state) == pawn(6, HexapawnState::Forward));
    }
    {
        ConsoleScope console("b1\nb2\n");
        PresentedConsoleBoard board = hexapawn_board();
        CHECK(board.poll_action(state) == pawn(7, HexapawnState::Forward));
        CHECK(console.output().find("Then: b2") != std::string::npos);
    }
}

TEST_CASE("PresentedConsoleBoard re-asks after an unknown word, takes back picks and understands undo")
{
    HexapawnState state;
    {
        ConsoleScope console("z9\nb1 b\nc1 c2\n");
        PresentedConsoleBoard board = hexapawn_board();
        CHECK(board.poll_action(state) == pawn(8, HexapawnState::Forward));
        CHECK(console.output().find("isn't a legal move") != std::string::npos);
    }
    {
        ConsoleScope console("a1 u\n");
        PresentedConsoleBoard board = hexapawn_board();
        CHECK(board.poll_action(state) == UNDO_ACTION);
    }
    {
        ConsoleScope console("a1\n");
        PresentedConsoleBoard board = hexapawn_board();
        CHECK(board.poll_action(state) == PENDING_ACTION);
        CHECK(std::cin.fail());
    }
}

TEST_CASE("create_console_board prefers a registered board, then a presenter, then the generic board")
{
    UniquePtr<IConsoleBoard> presented = create_console_board("tictactoe", k_all_seats);
    REQUIRE(presented != nullptr);
    CHECK(presented->shows_moves());

    ConsoleBoardRegistry::register_factory("tictactoe", [](const Params&) -> UniquePtr<IConsoleBoard> { return create_unique<ConsoleBoard>(); });
    CHECK_FALSE(create_console_board("tictactoe", k_all_seats)->shows_moves());
    ConsoleBoardRegistry::unregister_factory("tictactoe");
}

TEST_CASE("board_text lists the spaces of a layout that is not a text grid")
{
    BoardLayout2D builder;
    builder.add_space("low", { 0.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Square);
    builder.add_space("high", { 0.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Square);

    BoardView view;
    view.layout = builder.finish();
    view.pieces = { { 0, 0, 1 } };
    view.status = "over";
    std::vector<SpaceId> unchanged;
    BoardScene scene;
    build_board_scene(view, FakePresenter(), MoveBuilder(), { unchanged }, scene);
    CHECK(board_text(scene) == "low: .\nhigh: o\nover\n");
}
