#include "doctest.h"

#include "Oryx.h"

// Confirms OX_REGISTER_* actually ran at static-init time in the real binary, not just the generic Registry<T> (test_registry.cpp).
TEST_CASE("Oryx/Strategy's baseline strategies are registered under their documented names")
{
    CHECK(oryx::StrategyRegistry::has("random"));
    CHECK(oryx::StrategyRegistry::has("first-legal"));
    CHECK(oryx::StrategyRegistry::has("minimax"));
}

TEST_CASE("Oasis's game and strategy sources are compiled into Tests, so they self-register under their documented names")
{
    CHECK(oryx::GameRegistry::has("tictactoe"));
    CHECK(oryx::StrategyRegistry::has("tictactoe/heuristic"));
}
