#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

const ScriptOrigin kOrigin{ "lang", "mod", "" };

class ScriptedStrategyStub : public IScriptedStrategy
{
public:
    ActionId decide(const Context&) override { return INVALID_ACTION; }
    const ScriptOrigin& origin() const override { return kOrigin; }
};

class ScriptedGameFake : public IScriptedGame
{
public:
    UniquePtr<IState> new_initial_state() const override { return create_unique<DummyState>(5); }
    std::string name() const override { return "fake"; }
    int32_t num_players() const override { return 2; }
    const ScriptOrigin& origin() const override { return kOrigin; }
    const ParamSchema& param_schema() const override { return m_schema; }

private:
    ParamSchema m_schema;
};

std::string illegal_message(const IState& state, ActionId action, std::string_view context = {})
{
    try
    {
        check_legal(state, action, context);
    }
    catch (const ScriptError& error)
    {
        return error.what();
    }
    return "";
}

} // namespace

TEST_CASE("describe names a script origin and a script source")
{
    CHECK(describe(ScriptOrigin{ "python", "nim", "" }) == "python module 'nim'");
    CHECK(describe(ScriptOrigin{ "python", "nim", "scripts/nim.py" }) == "python module 'nim' (scripts/nim.py)");
    CHECK(describe(ScriptSource{ ScriptSourceKind::File, "a.py", "python" }) == "script 'a.py'");
    CHECK(describe(ScriptSource{ ScriptSourceKind::Module, "nim", "python" }) == "module 'nim'");
}

TEST_CASE("script_error joins context and message and carries the traceback as detail")
{
    ScriptError error = script_error("state.apply()", "boom", "trace");

    CHECK(std::string(error.what()) == "state.apply(): boom");
    CHECK(error.traceback() == "trace");
    CHECK(script_error("c", "m").traceback().empty());
}

TEST_CASE("check_legal accepts a listed action and rejects anything else with a ScriptError")
{
    DummyState state(5);

    CHECK_NOTHROW(check_legal(state, 1));
    CHECK_NOTHROW(check_legal(state, 3));
    CHECK(illegal_message(state, 4) == "action 4 is not legal in this state");
    CHECK(illegal_message(state, INVALID_ACTION).find("is not legal in this state") != std::string::npos);
}

TEST_CASE("check_legal prefixes the message with the context when one is given")
{
    DummyState state(5);
    CHECK(illegal_message(state, 9, "strategy.decide() returned an illegal action") == "strategy.decide() returned an illegal action: action 9 is not legal in this state");
}

TEST_CASE("check_legal rejects every action in a terminal state")
{
    DummyState state(0);
    CHECK(illegal_message(state, 1) == "action 1 is not legal in this state");
}

TEST_CASE("outcome_from_rewards builds an Outcome and rejects a wrong reward count")
{
    std::vector<double> rewards = { 1.0, -1.0 };
    Outcome outcome = outcome_from_rewards(rewards, 2, true);

    CHECK(outcome.is_terminal);
    REQUIRE(outcome.rewards.player_count() == 2);
    CHECK(outcome.rewards[0] == 1.0);
    CHECK(outcome.rewards[1] == -1.0);
    CHECK_FALSE(outcome_from_rewards(rewards, 2, false).is_terminal);

    CHECK_THROWS_WITH_AS(outcome_from_rewards(rewards, 3, true), "state.outcome(): expected 3 rewards but got 2", ScriptError);
    CHECK_THROWS_AS(outcome_from_rewards(std::vector<double>{}, 2, true), ScriptError);
}

TEST_CASE("rewards_to_vector is the inverse of outcome_from_rewards")
{
    std::vector<double> rewards = { 0.5, 0.25, -1.0 };
    CHECK(rewards_to_vector(outcome_from_rewards(rewards, 3, true).rewards) == rewards);
    CHECK(rewards_to_vector(Rewards<double>(0)).empty());
}

TEST_CASE("involves_script is true for a scripted game or any scripted strategy, false for an all-native match")
{
    DummyGame native_game(5);
    ScriptedGameFake scripted_game;
    ScriptedStrategyStub scripted;
    UniquePtr<IStrategy> native = StrategyRegistry::create("first-legal");
    REQUIRE(native != nullptr);

    std::vector<IStrategy*> native_only = { native.get(), native.get() };
    std::vector<IStrategy*> mixed = { native.get(), &scripted };

    CHECK_FALSE(involves_script(native_game, native_only));
    CHECK(involves_script(native_game, mixed));
    CHECK(involves_script(scripted_game, native_only));
    CHECK_FALSE(involves_script(native_game, std::span<IStrategy* const>{}));
}

TEST_CASE("seeded_params gives master_seed + seat to a strategy that declares a seed, and nothing to any other")
{
    Params first = seeded_params("random", 100, 0);
    Params second = seeded_params("random", 100, 1);

    REQUIRE(has_param(first, "seed"));
    CHECK(get_param<int64_t>(first, "seed") == 100);
    CHECK(get_param<int64_t>(second, "seed") == 101);

    CHECK(seeded_params("first-legal", 100, 0).empty());
    CHECK(seeded_params("no-such-strategy", 100, 0).empty());
}

TEST_CASE("a ScriptLease is invalid until a LeaseScope arms it, and the scope always revokes it")
{
    ScriptLease lease;
    CHECK_FALSE(lease.valid());
    CHECK_THROWS_WITH_AS(lease.require("context"), doctest::Contains("this context is no longer valid"), Error);

    {
        LeaseScope scope(lease);
        CHECK(lease.valid());
        CHECK_NOTHROW(lease.require("context"));
    }
    CHECK_FALSE(lease.valid());

    try
    {
        LeaseScope scope(lease);
        throw Error("body failed");
    }
    catch (const Error&)
    {
    }
    CHECK_FALSE(lease.valid());
}

TEST_CASE("a ScriptLease can be armed again for the next call")
{
    ScriptLease lease;
    for (int32_t call = 0; call < 3; ++call)
    {
        LeaseScope scope(lease);
        CHECK(lease.valid());
    }
    CHECK_FALSE(lease.valid());
}
