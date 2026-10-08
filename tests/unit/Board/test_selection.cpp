#include "doctest.h"

#include "BoardTestSupport.h"

#include "Oryx/Core/Registry.h"

using namespace oryx;
using namespace oryx::test;
using namespace oryx::selection;

namespace
{

struct ExtraGame
{
    ExtraGame()
    {
        GameRegistry::register_factory("zz-extra", [](const Params&) -> UniquePtr<IGame> { return create_unique<DummyGame>(5); });
    }

    ~ExtraGame() { GameRegistry::unregister_factory("zz-extra"); }
};

} // namespace

TEST_CASE("choose_game accepts a registered game and rejects an unknown one")
{
    std::string name;
    CHECK(choose_game("tictactoe", false, name));
    CHECK(name == "tictactoe");
    CHECK_FALSE(choose_game("no-such-game", false, name));
}

TEST_CASE("choose_game defaults to tictactoe without prompting when asked not to")
{
    ExtraGame extra;
    std::string name;
    CHECK(choose_game("", false, name));
    CHECK(name == "tictactoe");
}

TEST_CASE("choose_game prompts on stdin and exits when stdin closes")
{
    ExtraGame extra;
    std::string name;
    {
        ConsoleScope console("bogus\nzz-extra\n");
        CHECK(choose_game("", true, name));
        CHECK(name == "zz-extra");
        CHECK(console.output().find("Unrecognized choice") != std::string::npos);
    }
    {
        ConsoleScope console("");
        CHECK_FALSE(choose_game("", true, name));
    }
}

TEST_CASE("choose_opponent offers human and the game's strategies")
{
    std::vector<std::string> strategies = strategies_for("tictactoe");
    CHECK(std::find(strategies.begin(), strategies.end(), "random") != strategies.end());
    CHECK(std::find(strategies.begin(), strategies.end(), "tictactoe/heuristic") != strategies.end());
    std::vector<std::string> other = strategies_for("other");
    CHECK(std::find(other.begin(), other.end(), "tictactoe/heuristic") == other.end());

    std::string name;
    CHECK(choose_opponent("tictactoe", "human", false, name));
    CHECK(name == k_human_opponent);
    CHECK_FALSE(choose_opponent("tictactoe", "no-such-strategy", false, name));
    CHECK(choose_opponent("tictactoe", "", false, name));
    CHECK(name == "minimax");

    ConsoleScope console("random\n");
    CHECK(choose_opponent("tictactoe", "", true, name));
    CHECK(name == "random");
}

TEST_CASE("creatable_games lists the registered games sorted, and a new registration appears with no other change")
{
    std::vector<std::string> before = creatable_games();
    CHECK(std::is_sorted(before.begin(), before.end()));
    CHECK(std::find(before.begin(), before.end(), "tictactoe") != before.end());
    CHECK(std::find(before.begin(), before.end(), "zz-extra") == before.end());

    ExtraGame extra;
    std::vector<std::string> after = creatable_games();
    CHECK(after.size() == before.size() + 1);
    CHECK(std::find(after.begin(), after.end(), "zz-extra") != after.end());
}

TEST_CASE("opponents_for offers hot-seat first, then the strategies that fit the game")
{
    StrategyRegistry::register_factory("zz-other/strategy", [](const Params&) -> UniquePtr<IStrategy> { return nullptr; });
    std::vector<std::string> opponents = opponents_for("tictactoe");
    StrategyRegistry::unregister_factory("zz-other/strategy");
    REQUIRE_FALSE(opponents.empty());
    CHECK(opponents.front() == k_human_opponent);
    CHECK(std::find(opponents.begin(), opponents.end(), "minimax") != opponents.end());
    CHECK(std::find(opponents.begin(), opponents.end(), "zz-other/strategy") == opponents.end());
    CHECK(std::find(opponents.begin(), opponents.end(), "tictactoe/heuristic") != opponents.end());
}
