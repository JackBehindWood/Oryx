#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("oryx.make_game and make_strategy create registered C++ entries; list and describe report them")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("registry.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "game = oryx.make_game('tictactoe')\n"
        "mark(game.name() + '|' + str(game.num_players()) + '|')\n"
        "mark(str('tictactoe' in oryx.list_games()) + '|' + str('random' in oryx.list_strategies()) + '|')\n"
        "assert isinstance(oryx.make_strategy('random', seed=3), oryx.StrategyHandle)\n"
        "info = oryx.describe_strategy('random')\n"
        "param = info['params'][0]\n"
        "mark(info['description'] + '|' + param['name'] + '|' + param['type'] + '|' + str('default' in param) + '|')\n"
        "mark(str(oryx.describe_game('tictactoe')['params']))\n");

    RunningPython python;
    python.load(script);

    CHECK(read_file(marker) == "TicTacToe|2|True|True|Uniformly random legal action|seed|int|False|[]");
}

TEST_CASE("oryx.make_* raise OryxError for unknown names and ParamError naming the offending key")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("errors.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "def attempt(call):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.OryxError as e:\n"
        "        mark(type(e).__name__ + ':' + getattr(e, 'key', '-') + ':' + str(e) + ';')\n"
        "attempt(lambda: oryx.make_game('nope'))\n"
        "attempt(lambda: oryx.make_strategy('random', seed='x'))\n"
        "attempt(lambda: oryx.make_strategy('random', sed=1))\n"
        "attempt(lambda: oryx.make_strategy('random', seed=[1]))\n"
        "attempt(lambda: oryx.describe_strategy('nope'))\n");

    RunningPython python;
    python.load(script);

    std::string output = read_file(marker);
    CHECK(output.find("OryxError:-:no game is registered as 'nope'") != std::string::npos);
    CHECK(output.find("ParamError:seed:'random': parameter 'seed' expects int but got string;") != std::string::npos);
    CHECK(output.find("ParamError:sed:'random': parameter 'sed' is unknown") != std::string::npos);
    CHECK(output.find("ParamError:seed:'random': parameter 'seed' has unsupported type 'list'") != std::string::npos);
    CHECK(output.find("OryxError:-:no strategy is registered as 'nope'") != std::string::npos);
}

TEST_CASE("the registry, simulation and Random entry points raise OryxError before Oryx is initialised")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("guard.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "calls = {\n"
        "    'oryx.make_game': lambda: oryx.make_game('tictactoe'),\n"
        "    'oryx.make_strategy': lambda: oryx.make_strategy('random'),\n"
        "    'oryx.list_games': lambda: oryx.list_games(),\n"
        "    'oryx.list_strategies': lambda: oryx.list_strategies(),\n"
        "    'oryx.describe_game': lambda: oryx.describe_game('tictactoe'),\n"
        "    'oryx.describe_strategy': lambda: oryx.describe_strategy('random'),\n"
        "    'oryx.simulate': lambda: oryx.simulate('tictactoe', ['random', 'random']),\n"
        "    'oryx.Match': lambda: oryx.Match('tictactoe', ['random', 'random']),\n"
        "    'oryx.BatchRunner': lambda: oryx.BatchRunner('tictactoe', ['random', 'random']),\n"
        "}\n"
        "for name, call in calls.items():\n"
        "    try:\n"
        "        call()\n"
        "        mark(name + ' ran;')\n"
        "    except oryx.OryxError as e:\n"
        "        mark(str(e) + ';')\n");

    RunningPython python;
    {
        UninitialisedScope uninitialised;
        python.load(script);
    }

    std::string expected;
    for (const char* name : { "make_game", "make_strategy", "list_games", "list_strategies", "describe_game", "describe_strategy", "simulate", "Match", "BatchRunner" })
    {
        expected += std::string("oryx.") + name + "() was called before Oryx was initialised;";
    }
    CHECK(read_file(marker) == expected);
}

#endif
