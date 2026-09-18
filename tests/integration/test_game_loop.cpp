#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

Outcome play_dummy_game_to_terminal()
{
    DummyGame game(10);
    DummyGreedyStrategy strategy;
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);

    while (!state->is_terminal())
    {
        state->apply(strategy.decide(context));
    }
    return state->outcome();
}

} // namespace

// Relocated from tests/unit/Strategy/test_strategy.cpp: a full game+strategy
// loop to termination is DESIGN.md §11's "Integration tests: Game + strategy
// execution", not a Strategy unit test.
TEST_CASE("running DummyGreedyStrategy vs itself to terminal via a manual demo loop reaches a valid Outcome")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy;
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);

    while (!state->is_terminal())
    {
        ActionId action = strategy.decide(context);
        state->apply(action);
    }

    Outcome outcome = state->outcome();
    CHECK(outcome.is_terminal);
    CHECK(outcome.rewards.player_count() == 2);

    bool exactly_one_winner =
        (outcome.rewards[0] == doctest::Approx(1.0) && outcome.rewards[1] == doctest::Approx(-1.0)) ||
        (outcome.rewards[0] == doctest::Approx(-1.0) && outcome.rewards[1] == doctest::Approx(1.0));
    CHECK(exactly_one_winner);
}

TEST_CASE("Game + strategy loop is deterministic across repeated runs")
{
    Outcome first = play_dummy_game_to_terminal();
    Outcome second = play_dummy_game_to_terminal();

    CHECK(first.rewards[0] == doctest::Approx(second.rewards[0]));
    CHECK(first.rewards[1] == doctest::Approx(second.rewards[1]));
}
