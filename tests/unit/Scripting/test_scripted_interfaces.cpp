#include "doctest.h"

#include "Oryx.h"

namespace
{

const oryx::ScriptOrigin kOrigin{ "python", "nim", "scripts/nim.py" };

class StubScriptedState : public oryx::IScriptedState
{
public:
    oryx::ActionList legal_actions() const override { return {}; }
    void apply(oryx::ActionId) override {}
    void undo(oryx::ActionId) override {}
    oryx::PlayerId current_player() const override { return 0; }
    bool is_terminal() const override { return true; }
    oryx::Outcome outcome() const override { return {}; }
    std::string action_to_string(oryx::ActionId) const override { return "stub"; }
    const oryx::ScriptOrigin& origin() const override { return kOrigin; }
};

class StubScriptedGame : public oryx::IScriptedGame
{
public:
    oryx::UniquePtr<oryx::IState> new_initial_state() const override { return oryx::create_unique<StubScriptedState>(); }
    std::string name() const override { return "stub"; }
    int32_t num_players() const override { return 2; }
    const oryx::ScriptOrigin& origin() const override { return kOrigin; }
    const oryx::ParamSchema& param_schema() const override { return m_schema; }

private:
    oryx::ParamSchema m_schema = { oryx::int_param("stones", 21) };
};

class StubScriptedStrategy : public oryx::IScriptedStrategy
{
public:
    oryx::ActionId decide(const oryx::Context&) override { return oryx::INVALID_ACTION; }
    const oryx::ScriptOrigin& origin() const override { return kOrigin; }
};

} // namespace

TEST_CASE("Scripted game, state and strategy interfaces plug into the core interfaces")
{
    StubScriptedGame scripted_game;
    oryx::IGame& game = scripted_game;
    oryx::UniquePtr<oryx::IState> state = game.new_initial_state();
    REQUIRE(state != nullptr);
    CHECK(state->is_terminal());

    StubScriptedStrategy scripted_strategy;
    oryx::IStrategy& strategy = scripted_strategy;
    oryx::Context context(*state);
    CHECK(strategy.decide(context) == oryx::INVALID_ACTION);

    CHECK(scripted_game.origin() == kOrigin);
    CHECK(scripted_game.param_schema().size() == 1);
    CHECK(scripted_strategy.origin().language == "python");
}

TEST_CASE("ScriptOrigin and ScriptSource compare by value")
{
    CHECK(oryx::ScriptOrigin{ "python", "nim", "a.py" } == oryx::ScriptOrigin{ "python", "nim", "a.py" });
    CHECK(oryx::ScriptOrigin{ "python", "nim", "a.py" } != oryx::ScriptOrigin{ "python", "nim", "b.py" });
    CHECK(oryx::ScriptSource{ oryx::ScriptSourceKind::Module, "nim", "python" } != oryx::ScriptSource{ oryx::ScriptSourceKind::File, "nim", "python" });
}

TEST_CASE("ScriptError carries its traceback as the error detail")
{
    oryx::ScriptError error("boom", "Traceback (most recent call last)");

    CHECK(std::string(error.category()) == "script");
    CHECK(std::string(error.what()) == "boom");
    CHECK(error.traceback() == "Traceback (most recent call last)");
    CHECK_THROWS_AS(throw oryx::ScriptError("boom"), oryx::Error);
}
