#include "doctest.h"

#include "../Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

// Reuses the pattern from tests/unit/Game/test_capability.cpp - not shared
// directly since that one lives in an anonymous namespace there too (two
// call sites don't justify extracting a shared fixture yet, DESIGN.md §20).
class IMockCapability
{
public:
    virtual ~IMockCapability() = default;
    virtual int32_t mock_value() const = 0;
};

class MockCapability : public IMockCapability
{
public:
    int32_t mock_value() const override { return 42; }
};

class RequiresMockCapabilityStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override
    {
        const IMockCapability* capability = context.get<IMockCapability>();
        return capability != nullptr ? static_cast<ActionId>(capability->mock_value()) : INVALID_ACTION;
    }

    [[nodiscard]] std::vector<std::type_index> required_capabilities() const override
    {
        return { std::type_index(typeid(IMockCapability)) };
    }
};

class MockActionFeatures : public IActionFeatures
{
public:
    SmallVector<int32_t, 2> decode(ActionId action) const override { return { static_cast<int32_t>(action), 0 }; }
};

class MockFeatureGame : public DummyGame
{
public:
    explicit MockFeatureGame(uint32_t pile_size) : DummyGame(pile_size) {}

    IActionFeatures* action_features() const override
    {
        static MockActionFeatures instance;
        return &instance;
    }
};

} // namespace

TEST_CASE("Match::play() with two strategies reaches a terminal Outcome and records history")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    Match match(game, { &strategy_a, &strategy_b });

    Outcome outcome = match.play();

    CHECK(outcome.is_terminal);
    CHECK(match.history().size() > 0);

    // Replaying the recorded actions on a fresh state reaches the same outcome.
    UniquePtr<IState> replay_state = game.new_initial_state();
    for (ActionId action : match.history().actions())
    {
        replay_state->apply(action);
    }
    Outcome replayed = replay_state->outcome();
    CHECK(replayed.rewards[0] == doctest::Approx(outcome.rewards[0]));
    CHECK(replayed.rewards[1] == doctest::Approx(outcome.rewards[1]));
}

TEST_CASE("Match with an ExternalStrategy seat advances via decide()/apply() using externally-supplied actions")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;

    std::vector<ActionId> scripted = { 3, 3 };
    size_t next = 0;
    ExternalStrategy strategy_b([&](const Context&) -> ActionId { return scripted[next++]; });

    Match match(game, { &strategy_a, &strategy_b });

    CHECK(match.current_player() == 0);
    match.apply(match.decide());
    CHECK(match.current_player() == 1);

    ActionId action = match.decide();
    CHECK(action == 3);
    match.apply(action);

    CHECK(match.history().size() == 2);
}

TEST_CASE("Match::undo/redo reverse and reapply the last action in lockstep with IState")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    Match match(game, { &strategy_a, &strategy_b });

    PlayerId player_before = match.current_player();
    ActionId action = match.decide();
    match.apply(action);

    PlayerId player_after = match.current_player();
    CHECK(player_after != player_before);
    CHECK(match.history().can_undo());

    ActionId undone = match.undo();
    CHECK(undone == action);
    CHECK(match.current_player() == player_before);
    CHECK_FALSE(match.history().can_undo());
    CHECK(match.history().can_redo());

    ActionId redone = match.redo();
    CHECK(redone == action);
    CHECK(match.current_player() == player_after);
    CHECK_FALSE(match.history().can_redo());
}

TEST_CASE("Match::missing_capabilities flags a capability the Context doesn't provide")
{
    DummyState state(10);
    Context context(state);
    RequiresMockCapabilityStrategy strategy;

    std::vector<std::type_index> missing = Match::missing_capabilities(strategy, context);
    REQUIRE(missing.size() == 1);
    CHECK(missing[0] == std::type_index(typeid(IMockCapability)));

    MockCapability capability;
    context.provide<IMockCapability>(&capability);
    CHECK(Match::missing_capabilities(strategy, context).empty());
}

TEST_CASE("Match::build_context attaches IActionFeatures when the game provides one")
{
    MockFeatureGame game(10);
    UniquePtr<IState> state = game.new_initial_state();

    Context context = Match::build_context(game, *state);
    IActionFeatures* features = context.get<IActionFeatures>();
    REQUIRE(features != nullptr);
    CHECK(features->decode(5) == SmallVector<int32_t, 2>{ 5, 0 });
}

TEST_CASE("Match::build_context provides no IActionFeatures when the game doesn't have one")
{
    DummyGame game(10);
    UniquePtr<IState> state = game.new_initial_state();

    Context context = Match::build_context(game, *state);
    CHECK(context.get<IActionFeatures>() == nullptr);
}
