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
