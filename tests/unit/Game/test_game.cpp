#include "doctest.h"

#include "DummyGame.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("DummyGame::new_initial_state produces a non-terminal state with alternating legal actions")
{
    DummyGame game;
    auto state = game.new_initial_state();

    CHECK_FALSE(state->is_terminal());
    CHECK(state->current_player() == 0);

    auto actions = state->legal_actions();
    CHECK(actions.size() == 3);
}

TEST_CASE("DummyState::apply followed by undo restores pile size and current player exactly")
{
    DummyGame game(5);
    auto state = game.new_initial_state();

    ActionId action = 2;
    state->apply(action);
    CHECK(state->current_player() == 1);

    state->undo(action);
    CHECK(state->current_player() == 0);

    auto actions = state->legal_actions();
    CHECK(actions.size() == 3); // back to a pile of 5, all of take-1/2/3 legal
}

TEST_CASE("DummyState reaches is_terminal and reports zero-sum Outcome favoring the non-taking player")
{
    DummyGame game(1);
    auto state = game.new_initial_state();

    CHECK(state->legal_actions().size() == 1); // only "take 1" is legal

    state->apply(1); // player 0 takes the last stone and loses
    CHECK(state->is_terminal());

    Outcome outcome = state->outcome();
    CHECK(outcome.is_terminal);
    CHECK(outcome.rewards[0] == doctest::Approx(-1.0));
    CHECK(outcome.rewards[1] == doctest::Approx(1.0));
}

TEST_CASE("DummyState::action_to_string produces a human-readable action label")
{
    DummyGame game;
    auto state = game.new_initial_state();

    CHECK(state->action_to_string(2) == "take 2");
}
