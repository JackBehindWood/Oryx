#include "doctest.h"

#include "Oasis/Game/TicTacToeGame.h"
#include "Oasis/Strategy/TicTacToeHeuristicStrategy.h"

#include <initializer_list>

using namespace oryx;
using namespace oasis;

namespace
{

TicTacToeState state_after(std::initializer_list<ActionId> moves)
{
    TicTacToeState state;
    for (ActionId move : moves)
    {
        state.apply(move);
    }
    return state;
}

ActionId heuristic_choice(TicTacToeState& state)
{
    TicTacToeHeuristicStrategy strategy;
    Context context(state);
    return strategy.decide(context);
}

} // namespace

TEST_CASE("TicTacToeState starts empty with X to move and nine legal actions")
{
    TicTacToeState state;

    CHECK(state.current_player() == 0);
    CHECK_FALSE(state.is_terminal());
    CHECK(state.legal_actions().size() == 9);
    CHECK(state.outcome().rewards[0] == 0.0);
    CHECK(state.outcome().rewards[1] == 0.0);
}

TEST_CASE("TicTacToeState::apply alternates players and removes the cell from legal actions")
{
    TicTacToeState state = state_after({ 4 });
    ActionList legal = state.legal_actions();

    CHECK(state.current_player() == 1);
    CHECK(state.mark_at(1, 1) == Mark::X);
    CHECK(legal.size() == 8);
    for (ActionId action : legal)
    {
        CHECK(action != 4);
    }
}

TEST_CASE("TicTacToeState::undo restores the board and the player to move")
{
    TicTacToeState state = state_after({ 4, 0 });
    state.undo(0);

    CHECK(state.current_player() == 1);
    CHECK(state.mark_at(0, 0) == Mark::Empty);
    CHECK(state.legal_actions().size() == 8);
}

TEST_CASE("TicTacToeState detects a row win for X with reward +1/-1")
{
    TicTacToeState state = state_after({ 0, 3, 1, 4, 2 });
    Outcome outcome = state.outcome();

    CHECK(state.is_terminal());
    CHECK(outcome.rewards[0] == 1.0);
    CHECK(outcome.rewards[1] == -1.0);
}

TEST_CASE("TicTacToeState detects a column win for O")
{
    TicTacToeState state = state_after({ 0, 1, 3, 4, 8, 7 });
    Outcome outcome = state.outcome();

    CHECK(state.is_terminal());
    CHECK(outcome.rewards[1] == 1.0);
    CHECK(outcome.rewards[0] == -1.0);
}

TEST_CASE("TicTacToeState detects a diagonal win")
{
    TicTacToeState state = state_after({ 0, 1, 4, 2, 8 });

    CHECK(state.is_terminal());
    CHECK(state.outcome().rewards[0] == 1.0);
}

TEST_CASE("TicTacToeState reports a full board with no winner as a terminal draw")
{
    TicTacToeState state = state_after({ 0, 1, 2, 4, 3, 5, 7, 6, 8 });
    Outcome outcome = state.outcome();

    CHECK(state.is_terminal());
    CHECK(state.legal_actions().empty());
    CHECK(outcome.rewards[0] == 0.0);
    CHECK(outcome.rewards[1] == 0.0);
}

TEST_CASE("TicTacToeState::action_to_string is 1-based row/col")
{
    TicTacToeState state;

    CHECK(state.action_to_string(0) == "row 1, col 1");
    CHECK(state.action_to_string(5) == "row 2, col 3");
}

TEST_CASE("TicTacToeGame describes a two-player game and provides row/col action features")
{
    TicTacToeGame game;

    CHECK(game.name() == "TicTacToe");
    CHECK(game.num_players() == 2);
    REQUIRE(game.action_features() != nullptr);

    SmallVector<int32_t, 2> decoded = game.action_features()->decode(5);
    CHECK(decoded.size() == 2);
    CHECK(decoded[0] == 1);
    CHECK(decoded[1] == 2);
}

TEST_CASE("TicTacToeHeuristicStrategy takes an immediate win")
{
    TicTacToeState state = state_after({ 0, 3, 1, 4 });

    CHECK(heuristic_choice(state) == 2);
}

TEST_CASE("TicTacToeHeuristicStrategy blocks the opponent's winning cell when it cannot win itself")
{
    TicTacToeState state = state_after({ 0, 3, 8, 4 });

    CHECK(heuristic_choice(state) == 5);
}

TEST_CASE("TicTacToeHeuristicStrategy prefers the center, then a corner")
{
    TicTacToeState empty;
    CHECK(heuristic_choice(empty) == 4);

    TicTacToeState center_taken = state_after({ 4 });
    CHECK(heuristic_choice(center_taken) == 0);
}

TEST_CASE("Perfect play by MinimaxStrategy on both seats draws Tic-Tac-Toe")
{
    TicTacToeGame game;
    MinimaxStrategy first;
    MinimaxStrategy second;
    Match match(game, { &first, &second });

    Outcome outcome = match.play();

    CHECK(outcome.is_terminal);
    CHECK(outcome.rewards[0] == 0.0);
    CHECK(outcome.rewards[1] == 0.0);
}

TEST_CASE("ActionHistory's inline capacity holds the longest Tic-Tac-Toe game (9 plies) without touching the heap")
{
    constexpr size_t kMaxPlies = 9;
    static_assert(kActionHistoryInlineCapacity >= kMaxPlies);

    MemoryStats before = MemoryTracker::snapshot();
    ActionHistory history;
    for (ActionId action = 0; action < kMaxPlies; ++action)
    {
        history.record(action);
    }
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(history.size() == kMaxPlies);
    CHECK(delta.allocation_count == 0);
}
