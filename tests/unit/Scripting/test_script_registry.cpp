#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

const ScriptOrigin kNim{ "fake", "nim", "scripts/nim.fake" };
const ScriptOrigin kOther{ "fake", "other", "scripts/other.fake" };
const ScriptOrigin kLua{ "lua", "nim", "scripts/nim.lua" };

class LabelledGame : public IGame
{
public:
    explicit LabelledGame(std::string label)
        : m_label(std::move(label))
    {
    }

    UniquePtr<IState> new_initial_state() const override { return create_unique<DummyState>(1); }
    std::string name() const override { return m_label; }
    int32_t num_players() const override { return 2; }

private:
    std::string m_label;
};

GameRegistry::Factory labelled(const std::string& label)
{
    return [label](const Params&) -> UniquePtr<IGame> { return create_unique<LabelledGame>(label); };
}

std::string label_of(const std::string& id)
{
    UniquePtr<IGame> game = create_game(id);
    REQUIRE(game != nullptr);
    return game->name();
}

class ScriptRegistryCleanup
{
public:
    ~ScriptRegistryCleanup()
    {
        unregister_scripted("fake");
        unregister_scripted("lua");
    }
};

} // namespace

TEST_CASE("register_scripted_game adds a game to the registry and remembers its origin")
{
    ScriptRegistryCleanup cleanup;

    register_scripted_game("test-nim", kNim, labelled("v4"), EntryInfo{ { int_param("stones", 4) }, "A test game" });

    REQUIRE(GameRegistry::has("test-nim"));
    CHECK(GameRegistry::info("test-nim")->description == "A test game");
    REQUIRE(scripted_game_origin("test-nim") != nullptr);
    CHECK(*scripted_game_origin("test-nim") == kNim);
}

TEST_CASE("a C++ entry has no script origin")
{
    GameRegistry::register_factory("test-cpp-game", labelled("v1"));

    CHECK(scripted_game_origin("test-cpp-game") == nullptr);
    CHECK(scripted_game_origin("test-unregistered") == nullptr);
    GameRegistry::unregister_factory("test-cpp-game");
}

TEST_CASE("re-registering under the same origin replaces the entry")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-nim", kNim, labelled("v3"), {});

    register_scripted_game("test-nim", kNim, labelled("v5"), {});

    CHECK(label_of("test-nim") == "v5");
}

TEST_CASE("a clash with another origin or with C++ throws ScriptError unless overwrite is set")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-nim", kNim, labelled("v3"), {});
    GameRegistry::register_factory("test-cpp-game", labelled("v1"));

    CHECK_THROWS_AS(register_scripted_game("test-nim", kOther, labelled("v9"), {}), ScriptError);
    CHECK_THROWS_AS(register_scripted_game("test-nim", kLua, labelled("v9"), {}), ScriptError);
    CHECK_THROWS_AS(register_scripted_game("test-cpp-game", kNim, labelled("v9"), {}), ScriptError);
    CHECK(label_of("test-nim") == "v3");
    CHECK(label_of("test-cpp-game") == "v1");

    register_scripted_game("test-nim", kOther, labelled("v9"), {}, true);
    CHECK(label_of("test-nim") == "v9");
    CHECK(*scripted_game_origin("test-nim") == kOther);

    register_scripted_game("test-cpp-game", kNim, labelled("v7"), {}, true);
    CHECK(label_of("test-cpp-game") == "v7");
    CHECK(*scripted_game_origin("test-cpp-game") == kNim);
    GameRegistry::unregister_factory("test-cpp-game");
}

TEST_CASE("the clash error names the entry and the origin that owns it")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-nim", kNim, labelled("v3"), {});

    try
    {
        register_scripted_game("test-nim", kOther, labelled("v9"), {});
        FAIL("should have thrown");
    }
    catch (const ScriptError& error)
    {
        std::string message = error.what();
        CHECK(message.find("'test-nim'") != std::string::npos);
        CHECK(message.find("fake module 'nim' (scripts/nim.fake)") != std::string::npos);
    }
}

TEST_CASE("strategies are registered and tracked separately from games")
{
    ScriptRegistryCleanup cleanup;

    register_scripted_strategy("test-greedy", kNim, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, {});

    CHECK(StrategyRegistry::has("test-greedy"));
    CHECK(scripted_strategy_origin("test-greedy") != nullptr);
    CHECK(scripted_game_origin("test-greedy") == nullptr);
}

TEST_CASE("unregister_scripted drops only the entries of one language")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-fake-game", kNim, labelled("v3"), {});
    register_scripted_strategy("test-fake-strategy", kNim, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, {});
    register_scripted_game("test-lua-game", kLua, labelled("v3"), {});

    unregister_scripted("fake");

    CHECK_FALSE(GameRegistry::has("test-fake-game"));
    CHECK_FALSE(StrategyRegistry::has("test-fake-strategy"));
    CHECK(scripted_game_origin("test-fake-game") == nullptr);
    CHECK(GameRegistry::has("test-lua-game"));
}
