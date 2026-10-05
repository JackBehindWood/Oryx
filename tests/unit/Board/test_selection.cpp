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

struct FakeGraphicsBoardRegistration
{
    explicit FakeGraphicsBoardRegistration(std::string game)
        : m_game(std::move(game))
    {
        GraphicsBoardRegistry::register_factory(m_game, [](const Params&) -> UniquePtr<IGraphicsBoard> { return create_unique<FakeGraphicsBoard>(); });
    }

    ~FakeGraphicsBoardRegistration() { GraphicsBoardRegistry::unregister_factory(m_game); }

    std::string m_game;
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
    CHECK(name == kHumanOpponent);
    CHECK_FALSE(choose_opponent("tictactoe", "no-such-strategy", false, name));
    CHECK(choose_opponent("tictactoe", "", false, name));
    CHECK(name == "minimax");

    ConsoleScope console("random\n");
    CHECK(choose_opponent("tictactoe", "", true, name));
    CHECK(name == "random");
}

TEST_CASE("choose_front_end is Console when headless or graphics are not built")
{
    FakeGraphicsBoardRegistration board("tictactoe");
    FrontEnd front_end = FrontEnd::Graphical;
    std::string game;

    CHECK(choose_front_end("", true, true, front_end, game));
    CHECK(front_end == FrontEnd::Console);

    CHECK(choose_front_end("", false, false, front_end, game));
    CHECK(front_end == FrontEnd::Console);
}

TEST_CASE("choose_front_end is Graphical only for a game with a graphics board, and resolves the game")
{
    FrontEnd front_end = FrontEnd::Console;
    std::string game;
    {
        FakeGraphicsBoardRegistration board("tictactoe");
        CHECK(choose_front_end("", false, true, front_end, game));
        CHECK(front_end == FrontEnd::Graphical);
        CHECK(game == "tictactoe");
    }

    CHECK(choose_front_end("", false, true, front_end, game));
    CHECK(front_end == FrontEnd::Console);
    CHECK_FALSE(choose_front_end("no-such-game", false, true, front_end, game));
}
