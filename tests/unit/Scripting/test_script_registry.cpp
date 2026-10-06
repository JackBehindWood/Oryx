#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

const ScriptOrigin k_nim{ "fake", "nim", "scripts/nim.fake" };
const ScriptOrigin k_other{ "fake", "other", "scripts/other.fake" };
const ScriptOrigin k_lua{ "lua", "nim", "scripts/nim.lua" };

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

    register_scripted_game("test-nim", k_nim, labelled("v4"), EntryInfo{ { int_param("stones", 4) }, "A test game" });

    REQUIRE(GameRegistry::has("test-nim"));
    CHECK(GameRegistry::info("test-nim")->description == "A test game");
    REQUIRE(scripted_game_origin("test-nim") != nullptr);
    CHECK(*scripted_game_origin("test-nim") == k_nim);
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
    register_scripted_game("test-nim", k_nim, labelled("v3"), {});

    register_scripted_game("test-nim", k_nim, labelled("v5"), {});

    CHECK(label_of("test-nim") == "v5");
}

TEST_CASE("a clash with another origin or with C++ throws ScriptError unless overwrite is set")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-nim", k_nim, labelled("v3"), {});
    GameRegistry::register_factory("test-cpp-game", labelled("v1"));

    CHECK_THROWS_AS(register_scripted_game("test-nim", k_other, labelled("v9"), {}), ScriptError);
    CHECK_THROWS_AS(register_scripted_game("test-nim", k_lua, labelled("v9"), {}), ScriptError);
    CHECK_THROWS_AS(register_scripted_game("test-cpp-game", k_nim, labelled("v9"), {}), ScriptError);
    CHECK(label_of("test-nim") == "v3");
    CHECK(label_of("test-cpp-game") == "v1");

    register_scripted_game("test-nim", k_other, labelled("v9"), {}, true);
    CHECK(label_of("test-nim") == "v9");
    CHECK(*scripted_game_origin("test-nim") == k_other);

    register_scripted_game("test-cpp-game", k_nim, labelled("v7"), {}, true);
    CHECK(label_of("test-cpp-game") == "v7");
    CHECK(*scripted_game_origin("test-cpp-game") == k_nim);
    GameRegistry::unregister_factory("test-cpp-game");
}

TEST_CASE("the clash error names the entry and the origin that owns it")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-nim", k_nim, labelled("v3"), {});

    try
    {
        register_scripted_game("test-nim", k_other, labelled("v9"), {});
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

    register_scripted_strategy("test-greedy", k_nim, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, {});

    CHECK(StrategyRegistry::has("test-greedy"));
    CHECK(scripted_strategy_origin("test-greedy") != nullptr);
    CHECK(scripted_game_origin("test-greedy") == nullptr);
}

TEST_CASE("overwriting a C++ entry restores it once the scripted runtime unregisters")
{
    ScriptRegistryCleanup cleanup;
    StrategyRegistry::register_factory("test-cpp-strategy", [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, EntryInfo{ {}, "original C++" });

    register_scripted_strategy("test-cpp-strategy", k_nim, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, EntryInfo{ {}, "scripted" }, true);
    CHECK(StrategyRegistry::info("test-cpp-strategy")->description == "scripted");
    CHECK(scripted_strategy_origin("test-cpp-strategy") != nullptr);

    unregister_scripted("fake");

    REQUIRE(StrategyRegistry::has("test-cpp-strategy"));
    CHECK(StrategyRegistry::info("test-cpp-strategy")->description == "original C++");
    CHECK(scripted_strategy_origin("test-cpp-strategy") == nullptr);

    StrategyRegistry::unregister_factory("test-cpp-strategy");
}

TEST_CASE("a same-language chain of overwrites restores the C++ entry")
{
    ScriptRegistryCleanup cleanup;
    GameRegistry::register_factory("test-chain", labelled("cpp"));

    register_scripted_game("test-chain", k_nim, labelled("a"), {}, true);
    register_scripted_game("test-chain", k_other, labelled("b"), {}, true);
    unregister_scripted("fake");

    CHECK(label_of("test-chain") == "cpp");
    CHECK(scripted_game_origin("test-chain") == nullptr);
    GameRegistry::unregister_factory("test-chain");
}

TEST_CASE("a mixed chain of overwrites restores the immediately-clobbered entry")
{
    ScriptRegistryCleanup cleanup;
    StrategyRegistry::register_factory("test-chain", [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, EntryInfo{ {}, "original C++" });

    register_scripted_strategy("test-chain", k_nim, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, EntryInfo{ {}, "fake" }, true);
    register_scripted_strategy("test-chain", k_lua, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, EntryInfo{ {}, "lua" }, true);

    unregister_scripted("lua");

    REQUIRE(StrategyRegistry::has("test-chain"));
    CHECK(StrategyRegistry::info("test-chain")->description == "fake");
    REQUIRE(scripted_strategy_origin("test-chain") != nullptr);
    CHECK(*scripted_strategy_origin("test-chain") == k_nim);

    unregister_scripted("fake");

    REQUIRE(StrategyRegistry::has("test-chain"));
    CHECK(StrategyRegistry::info("test-chain")->description == "original C++");
    CHECK(scripted_strategy_origin("test-chain") == nullptr);
    StrategyRegistry::unregister_factory("test-chain");
}

TEST_CASE("unregistering a language leaves none of its layers under another language's entry")
{
    ScriptRegistryCleanup cleanup;
    GameRegistry::register_factory("test-chain", labelled("cpp"));

    register_scripted_game("test-chain", k_nim, labelled("fake-1"), {}, true);
    register_scripted_game("test-chain", k_lua, labelled("lua"), {}, true);
    register_scripted_game("test-chain", k_other, labelled("fake-2"), {}, true);

    unregister_scripted("fake");
    CHECK(label_of("test-chain") == "lua");
    REQUIRE(scripted_game_origin("test-chain") != nullptr);
    CHECK(*scripted_game_origin("test-chain") == k_lua);

    unregister_scripted("lua");
    CHECK(label_of("test-chain") == "cpp");
    CHECK(scripted_game_origin("test-chain") == nullptr);
    GameRegistry::unregister_factory("test-chain");
}

TEST_CASE("an origin re-registering over its own clobberer never restores its earlier definition")
{
    ScriptRegistryCleanup cleanup;
    GameRegistry::register_factory("test-chain", labelled("cpp"));

    register_scripted_game("test-chain", k_nim, labelled("fake-old"), {}, true);
    register_scripted_game("test-chain", k_lua, labelled("lua"), {}, true);
    register_scripted_game("test-chain", k_nim, labelled("fake-new"), {}, true);

    unregister_scripted("lua");
    CHECK(label_of("test-chain") == "fake-new");

    unregister_scripted("fake");
    CHECK(label_of("test-chain") == "cpp");
    GameRegistry::unregister_factory("test-chain");
}

TEST_CASE("reloading and then deleting the scripts of an overwrite chain brings the C++ entry back")
{
    ScriptRegistryCleanup cleanup;
    GameRegistry::register_factory("test-chain", labelled("cpp"));
    register_scripted_game("test-chain", k_nim, labelled("a"), {}, true);
    register_scripted_game("test-chain", k_other, labelled("b"), {}, true);

    unregister_scripted("fake");
    register_scripted_game("test-chain", k_nim, labelled("a"), {}, true);
    register_scripted_game("test-chain", k_other, labelled("b"), {}, true);
    CHECK(label_of("test-chain") == "b");

    unregister_scripted("fake");

    CHECK(label_of("test-chain") == "cpp");
    CHECK(scripted_game_origin("test-chain") == nullptr);
    GameRegistry::unregister_factory("test-chain");
}

TEST_CASE("a scripted entry removed directly from the registry leaves no origin or stash behind")
{
    ScriptRegistryCleanup cleanup;
    GameRegistry::register_factory("test-chain", labelled("cpp"));
    register_scripted_game("test-chain", k_nim, labelled("a"), {}, true);

    GameRegistry::unregister_factory("test-chain");
    CHECK(scripted_game_origin("test-chain") == nullptr);

    unregister_scripted("fake");
    CHECK_FALSE(GameRegistry::has("test-chain"));

    GameRegistry::register_factory("test-chain", labelled("cpp-2"));
    CHECK_THROWS_AS(register_scripted_game("test-chain", k_nim, labelled("a"), {}), ScriptError);
    GameRegistry::unregister_factory("test-chain");
}

TEST_CASE("unregister_scripted drops only the entries of one language")
{
    ScriptRegistryCleanup cleanup;
    register_scripted_game("test-fake-game", k_nim, labelled("v3"), {});
    register_scripted_strategy("test-fake-strategy", k_nim, [](const Params&) -> UniquePtr<IStrategy> { return create_unique<DummyGreedyStrategy>(); }, {});
    register_scripted_game("test-lua-game", k_lua, labelled("v3"), {});

    unregister_scripted("fake");

    CHECK_FALSE(GameRegistry::has("test-fake-game"));
    CHECK_FALSE(StrategyRegistry::has("test-fake-strategy"));
    CHECK(scripted_game_origin("test-fake-game") == nullptr);
    CHECK(GameRegistry::has("test-lua-game"));
}

namespace
{

class LabelledBoard : public IConsoleBoard
{
public:
    explicit LabelledBoard(bool shows)
        : m_shows(shows)
    {
    }

    void on_turn(const IState&) override {}
    ActionId poll_action(const IState&) override { return PENDING_ACTION; }
    bool shows_moves() const override { return m_shows; }

private:
    bool m_shows;
};

ConsoleBoardRegistry::Factory board_showing(bool shows)
{
    return [shows](const Params&) -> UniquePtr<IConsoleBoard> { return create_unique<LabelledBoard>(shows); };
}

bool shows_moves_of(const std::string& game)
{
    UniquePtr<IConsoleBoard> board = ConsoleBoardRegistry::create(game);
    REQUIRE(board != nullptr);
    return board->shows_moves();
}

} // namespace

TEST_CASE("register_scripted_console_board adds a board for a game and remembers its origin")
{
    ScriptRegistryCleanup cleanup;

    register_scripted_console_board("test-board", k_nim, board_showing(true), {});
    CHECK(shows_moves_of("test-board"));
    REQUIRE(scripted_console_board_origin("test-board") != nullptr);
    CHECK(*scripted_console_board_origin("test-board") == k_nim);

    register_scripted_console_board("test-board", k_nim, board_showing(false), {});
    CHECK_FALSE(shows_moves_of("test-board"));

    unregister_scripted("fake");
    CHECK_FALSE(ConsoleBoardRegistry::has("test-board"));
    CHECK(scripted_console_board_origin("test-board") == nullptr);
}

TEST_CASE("a console board from another origin needs overwrite=True, and unregistering restores the board it replaced")
{
    ScriptRegistryCleanup cleanup;

    ConsoleBoardRegistry::register_factory("test-board-cpp", board_showing(true));
    CHECK_THROWS_AS(register_scripted_console_board("test-board-cpp", k_nim, board_showing(false), {}), ScriptError);
    CHECK(shows_moves_of("test-board-cpp"));

    register_scripted_console_board("test-board-cpp", k_nim, board_showing(false), {}, true);
    CHECK_FALSE(shows_moves_of("test-board-cpp"));

    CHECK_THROWS_AS(register_scripted_console_board("test-board-cpp", k_other, board_showing(true), {}), ScriptError);

    unregister_scripted("fake");
    CHECK(shows_moves_of("test-board-cpp"));
    CHECK(scripted_console_board_origin("test-board-cpp") == nullptr);

    ConsoleBoardRegistry::unregister_factory("test-board-cpp");
}
