#include "doctest.h"

#include "../Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("DummyGreedyStrategy always selects the maximum legal action")
{
    DummyGame game(10);
    auto state = game.new_initial_state();

    DummyGreedyStrategy strategy;
    ActionId action = strategy.decide(*state);

    CHECK(action == 3);
}

TEST_CASE("running DummyGreedyStrategy vs itself to terminal via a manual demo loop reaches a valid Outcome")
{
    DummyGame game(10);
    auto state = game.new_initial_state();

    DummyGreedyStrategy strategy;

    while (!state->is_terminal())
    {
        ActionId action = strategy.decide(*state);
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
