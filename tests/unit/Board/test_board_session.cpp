#include "doctest.h"

#include "BoardTestSupport.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("BoardSession forwards turns to its board and tracks the game ending once")
{
    SharedPtr<FakeBoard> board = create_shared<FakeBoard>();
    BoardSession session(board, false);

    UniquePtr<IState> running = DummyGame(5).new_initial_state();
    session.on_turn(*running);
    CHECK(board->turns == 1);
    CHECK_FALSE(session.game_over());

    UniquePtr<IState> finished = finished_dummy_state();
    session.on_turn(*finished);
    session.on_turn(*finished);
    CHECK(board->turns == 3);
    CHECK(session.game_over());
}

TEST_CASE("BoardSession announces the outcome on the terminal exactly once when asked to")
{
    ConsoleScope console("");
    BoardSession session(create_shared<FakeBoard>(), true);

    UniquePtr<IState> finished = finished_dummy_state();
    session.on_turn(*finished);
    session.on_turn(*finished);

    std::string output = console.output();
    CHECK(output.find("wins") != std::string::npos);
    CHECK(output.find("wins") == output.rfind("wins"));
}

TEST_CASE("BoardSession stays quiet when not asked to announce")
{
    ConsoleScope console("");
    BoardSession session(create_shared<FakeBoard>(), false);
    session.on_turn(*finished_dummy_state());
    CHECK(console.output().empty());
}

TEST_CASE("BoardSession is ready to restart only after the finished game has been shown for the delay")
{
    BoardSession session(create_shared<FakeBoard>(), false);

    session.advance(1.0);
    CHECK_FALSE(session.restart_ready());

    session.on_turn(*finished_dummy_state());
    CHECK_FALSE(session.restart_ready());
    session.advance(BoardSession::kRestartDelaySeconds * 0.5);
    CHECK_FALSE(session.restart_ready());
    session.advance(BoardSession::kRestartDelaySeconds);
    CHECK(session.restart_ready());

    session.restart();
    CHECK_FALSE(session.game_over());
    CHECK_FALSE(session.restart_ready());
}

TEST_CASE("BoardSession serves human seats from the board's poll_action")
{
    SharedPtr<FakeBoard> board = create_shared<FakeBoard>();
    BoardSession session(board, false);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    Context context(*state);

    CHECK(session.next_action(context) == PENDING_ACTION);
    board->next_action = 2;
    CHECK(session.next_action(context) == 2);
}
