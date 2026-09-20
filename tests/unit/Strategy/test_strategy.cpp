#include "doctest.h"

#include "../Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("DummyGreedyStrategy always selects the maximum legal action")
{
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    DummyGreedyStrategy strategy;
    ActionId action = strategy.decide(context);

    CHECK(action == 3);
}

TEST_CASE("RandomStrategy always returns a legal action")
{
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    RandomStrategy strategy(/*seed=*/1);
    for (int i = 0; i < 50; ++i)
    {
        ActionId action = strategy.decide(context);
        auto legal = state->legal_actions();
        CHECK(std::find(legal.begin(), legal.end(), action) != legal.end());
    }
}

TEST_CASE("RandomStrategy is deterministic for a given seed")
{
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    RandomStrategy first(/*seed=*/42);
    RandomStrategy second(/*seed=*/42);

    for (int i = 0; i < 10; ++i)
    {
        CHECK(first.decide(context) == second.decide(context));
    }
}

TEST_CASE("FirstLegalStrategy always selects the first legal action")
{
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    FirstLegalStrategy strategy;
    ActionId action = strategy.decide(context);

    CHECK(action == 1);
}

TEST_CASE("MinimaxStrategy finds the only optimal move from a pile of 2")
{
    // Misère take-1..3 Nim: mover who takes the last stone loses. P-positions
    // (mover loses under optimal play) are piles ≡ 1 (mod 4). From pile=2,
    // the only move to a P-position (pile=1) is take=1.
    DummyGame game(2);
    auto state = game.new_initial_state();
    Context context(*state);

    MinimaxStrategy strategy;
    CHECK(strategy.decide(context) == 1);
}

TEST_CASE("MinimaxStrategy finds the only optimal move from a pile of 10")
{
    // From pile=10, the only move to a P-position (pile=9, since 9 mod 4 == 1)
    // is take=1.
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    MinimaxStrategy strategy;
    CHECK(strategy.decide(context) == 1);
}

TEST_CASE("The registered random strategy takes a seed param and matches a directly seeded RandomStrategy")
{
    DummyGame game(10);
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);

    UniquePtr<IStrategy> from_registry = StrategyRegistry::create("random", { { "seed", int64_t{ 42 } } });
    UniquePtr<IStrategy> other_from_registry = StrategyRegistry::create("random", { { "seed", int64_t{ 42 } } });
    RandomStrategy direct(/*seed=*/42);
    REQUIRE(from_registry != nullptr);
    REQUIRE(other_from_registry != nullptr);

    for (int32_t i = 0; i < 10; ++i)
    {
        ActionId expected = direct.decide(context);
        CHECK(from_registry->decide(context) == expected);
        CHECK(other_from_registry->decide(context) == expected);
    }
}

TEST_CASE("The registered random strategy can be created without a seed")
{
    CHECK(StrategyRegistry::create("random") != nullptr);
}

TEST_CASE("The registered random strategy rejects an unknown param naming the key")
{
    CHECK_THROWS_WITH_AS(StrategyRegistry::create("random", { { "sed", int64_t{ 1 } } }), doctest::Contains("'sed'"), ParamError);
    CHECK_THROWS_AS(StrategyRegistry::create("random", { { "seed", std::string("one") } }), ParamError);
}

TEST_CASE("Strategies without params reject any param")
{
    CHECK_THROWS_WITH_AS(StrategyRegistry::create("minimax", { { "depth", int64_t{ 3 } } }), doctest::Contains("'depth'"), ParamError);
    CHECK_THROWS_AS(StrategyRegistry::create("first-legal", { { "depth", int64_t{ 3 } } }), ParamError);
}
