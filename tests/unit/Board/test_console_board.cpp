#include "doctest.h"

#include "BoardTestSupport.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("read_console_move lists the legal moves and returns the chosen one")
{
    ConsoleScope console("2\n");
    UniquePtr<IState> state = DummyGame(5).new_initial_state();

    CHECK(read_console_move(*state) == 2);
    CHECK(console.output().find("take") != std::string::npos);
}

TEST_CASE("read_console_move re-asks for an illegal or unparsable move and understands undo")
{
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    {
        ConsoleScope console("x\n9\n3\n");
        CHECK(read_console_move(*state) == 3);
        CHECK(console.output().find("isn't a legal move") != std::string::npos);
    }
    {
        ConsoleScope console("u\n");
        CHECK(read_console_move(*state) == UNDO_ACTION);
    }
}

TEST_CASE("read_console_move returns PENDING_ACTION, never INVALID_ACTION, once stdin is exhausted")
{
    ConsoleScope console("");
    UniquePtr<IState> state = DummyGame(5).new_initial_state();

    CHECK(read_console_move(*state) == PENDING_ACTION);
    CHECK(std::cin.fail());
    CHECK(read_console_move(*state) == PENDING_ACTION);
}

TEST_CASE("create_console_board falls back to the generic ConsoleBoard for a game without one")
{
    UniquePtr<IConsoleBoard> board = create_console_board("no-such-game", k_all_seats);
    REQUIRE(board != nullptr);
    CHECK_FALSE(board->shows_moves());
}

TEST_CASE("print_console_outcome names the winner or a draw")
{
    Outcome win;
    win.rewards = Rewards<double>(2);
    win.rewards[1] = 1.0;
    win.rewards[0] = -1.0;
    {
        ConsoleScope console("");
        print_console_outcome(win);
        CHECK(console.output() == "Player 2 wins!\n");
    }

    Outcome draw;
    draw.rewards = Rewards<double>(2);
    ConsoleScope console("");
    print_console_outcome(draw);
    CHECK(console.output() == "It's a draw!\n");
    CHECK(winner_of(draw) == -1);
    CHECK(winner_of(win) == 1);
}
