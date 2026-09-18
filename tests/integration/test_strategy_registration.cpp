#include "doctest.h"

#include "Oryx.h"

// Confirms OX_REGISTER_STRATEGY actually ran at static-init time in the real
// binary - not just the generic Registry<T> mechanism (already covered by
// test_registry.cpp's DummyRegistry).
TEST_CASE("Oryx/Strategy's baseline strategies are registered under their documented names")
{
    CHECK(oryx::StrategyRegistry::has("random"));
    CHECK(oryx::StrategyRegistry::has("first-legal"));
    CHECK(oryx::StrategyRegistry::has("minimax"));
}

TEST_CASE("Oasis is not linked into the Tests binary, so its game/strategy never register here")
{
    CHECK_FALSE(oryx::GameRegistry::has("tictactoe"));
    CHECK_FALSE(oryx::StrategyRegistry::has("tictactoe/heuristic"));
}
