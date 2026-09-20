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

TEST_CASE("Registered entries carry the schema and description they declared")
{
    const oryx::EntryInfo* random = oryx::StrategyRegistry::info("random");
    REQUIRE(random != nullptr);
    REQUIRE(random->schema.size() == 1);
    CHECK(random->schema[0].name == "seed");
    CHECK(random->schema[0].type == oryx::ParamType::Int);
    CHECK_FALSE(random->schema[0].has_default);
    CHECK_FALSE(random->description.empty());

    const oryx::EntryInfo* tictactoe = oryx::GameRegistry::info("tictactoe");
    REQUIRE(tictactoe != nullptr);
    CHECK(tictactoe->schema.empty());
    CHECK_FALSE(tictactoe->description.empty());
}

TEST_CASE("create_game rejects a param the game does not declare")
{
    CHECK(oryx::create_game("tictactoe") != nullptr);
    CHECK_THROWS_WITH_AS(oryx::create_game("tictactoe", { { "size", int64_t{ 4 } } }), doctest::Contains("'size'"), oryx::ParamError);
}
