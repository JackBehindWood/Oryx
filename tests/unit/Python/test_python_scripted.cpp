#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

extern "C" int PyGILState_Check(void);

namespace
{

const char* const kNim =
    "import oryx\n"
    "\n"
    "class NimState:\n"
    "    def __init__(self, stones, max_take):\n"
    "        self.stones = stones\n"
    "        self.max_take = max_take\n"
    "        self.player = 0\n"
    "    def legal_actions(self):\n"
    "        return list(range(1, min(self.max_take, self.stones) + 1))\n"
    "    def apply(self, action):\n"
    "        self.stones -= action\n"
    "        self.player = 1 - self.player\n"
    "    def undo(self, action):\n"
    "        self.stones += action\n"
    "        self.player = 1 - self.player\n"
    "    def current_player(self):\n"
    "        return self.player\n"
    "    def is_terminal(self):\n"
    "        return self.stones == 0\n"
    "    def outcome(self):\n"
    "        rewards = [0.0, 0.0]\n"
    "        if self.stones == 0:\n"
    "            rewards = [-1.0, -1.0]\n"
    "            rewards[1 - self.player] = 1.0\n"
    "        return rewards\n"
    "    def action_to_string(self, action):\n"
    "        return f'take {action}'\n"
    "\n"
    "class Nim(oryx.Game, id='nim'):\n"
    "    '''Take turns removing stones; the last taker wins.\n"
    "\n"
    "    More text that describe() leaves out.'''\n"
    "    stones: int = 21\n"
    "    max_take: int = 3\n"
    "    num_players = 2\n"
    "    def new_initial_state(self):\n"
    "        return NimState(self.stones, self.max_take)\n";

const char* const kMonteCarlo =
    "class MonteCarlo(oryx.Strategy, id='monte-carlo'):\n"
    "    '''Flat Monte Carlo: random playouts for every legal action.'''\n"
    "    playouts: int = 30\n"
    "    seed: int = 1\n"
    "    def __init__(self):\n"
    "        self.random = oryx.Random(self.seed)\n"
    "    def decide(self, context):\n"
    "        state = context.state\n"
    "        me = state.current_player()\n"
    "        best, best_total = None, None\n"
    "        for action in state.legal_actions():\n"
    "            state.apply(action)\n"
    "            total = sum(self.playout(state, me) for _ in range(self.playouts))\n"
    "            state.undo(action)\n"
    "            if best_total is None or total > best_total:\n"
    "                best, best_total = action, total\n"
    "        return best\n"
    "    def playout(self, state, me):\n"
    "        applied = []\n"
    "        while not state.is_terminal():\n"
    "            actions = state.legal_actions()\n"
    "            action = actions[self.random.get_int(0, len(actions) - 1)]\n"
    "            state.apply(action)\n"
    "            applied.append(action)\n"
    "        reward = state.outcome()[me]\n"
    "        for action in reversed(applied):\n"
    "            state.undo(action)\n"
    "        return reward\n";

std::string run_script(const std::string& body, const std::string& file_name = "nim.py")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write(file_name, marker_prelude(marker) + body);

    RunningPython python;
    python.load(script);
    return read_file(marker);
}

std::string with_nim(const std::string& body)
{
    return std::string(kNim) + body;
}

bool g_probe_saw_gil = false;

class GilProbeStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override
    {
        g_probe_saw_gil = PyGILState_Check() != 0;
        return context.state().legal_actions()[0];
    }
};

} // namespace

TEST_CASE("a Python strategy plays a C++ game and is described with its description")
{
    std::string output = run_script(std::string(
        "import oryx\n"
        "class LastLegal(oryx.Strategy, id='last-legal'):\n"
        "    '''Always the last legal action.'''\n"
        "    def decide(self, context):\n"
        "        return context.state.legal_actions()[-1]\n"
        "match = oryx.Match('tictactoe', ['last-legal', 'first-legal'])\n"
        "mark(str(match.play()) + str(match.history()) + '|')\n"
        "match = oryx.Match('tictactoe', [LastLegal(), oryx.make_strategy('first-legal')])\n"
        "mark(str(match.play()) + '|' + oryx.describe_strategy('last-legal')['description'])\n"));

    CHECK(output == "[1.0, -1.0][8, 0, 7, 1, 6]|[1.0, -1.0]|Always the last legal action.");
}

TEST_CASE("one Python Monte Carlo strategy beats a naive C++ strategy at both a C++ game and a Python game")
{
    std::string output = run_script(with_nim(std::string(kMonteCarlo) +
        "tictactoe = oryx.simulate('tictactoe', ['monte-carlo', 'first-legal'], games=10)\n"
        "nim = oryx.simulate('nim', ['monte-carlo', 'first-legal'], games=10)\n"
        "mark(f'{tictactoe.wins[0]}|{nim.wins[0]}')\n"));

    size_t separator = output.find('|');
    REQUIRE(separator != std::string::npos);
    CHECK(std::stoi(output.substr(0, separator)) >= 8);
    CHECK(std::stoi(output.substr(separator + 1)) >= 8);
}

TEST_CASE("the Oasis example scripts register Nim and a Monte Carlo strategy that plays both games")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path check = dir.write("check.py", marker_prelude(marker) +
        "import oryx\n"
        "nim = oryx.simulate('nim', ['monte-carlo', 'first-legal'], games=10)\n"
        "tictactoe = oryx.simulate('tictactoe', ['monte-carlo', 'first-legal'], games=10)\n"
        "mark(f'{nim.wins[0]}|{tictactoe.wins[0]}|' + oryx.describe_game('nim')['origin']['source_file'])\n");

    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.py"));
    python.load(repo_file("Oasis/scripts/monte_carlo.py"));
    python.load(check);

    std::string output = read_file(marker);
    size_t first = output.find('|');
    size_t second = output.find('|', first + 1);
    REQUIRE(second != std::string::npos);
    CHECK(std::stoi(output.substr(0, first)) >= 8);
    CHECK(std::stoi(output.substr(first + 1, second - first - 1)) >= 8);
    CHECK(output.find("Oasis/scripts/nim.py") != std::string::npos);
}

TEST_CASE("re-running the same script replaces its entries; another origin needs overwrite=True")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::string report = "mark(str(oryx.describe_game('nim')['params'][0]['default']) + ';')\n";
    std::filesystem::path script = dir.write("nim.py", marker_prelude(marker) + kNim + report);

    RunningPython python;
    python.load(script);
    CHECK(read_file(marker) == "21;");

    std::string edited = kNim;
    edited.replace(edited.find("stones: int = 21"), std::string("stones: int = 21").size(), "stones: int = 5");
    dir.write("nim.py", marker_prelude(marker) + edited + report);
    python.reload(script);
    CHECK(read_file(marker) == "21;5;");
    std::vector<std::string> names = GameRegistry::names();
    CHECK(std::count(names.begin(), names.end(), "nim") == 1);

    std::filesystem::path rival = dir.write("rival.py",
        "import oryx\n"
        "class Rival(oryx.Game, id='nim'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n");
    try
    {
        python.load(rival);
        FAIL("the clash should have thrown");
    }
    catch (const ScriptError& error)
    {
        CHECK(error.traceback().find("the game 'nim' is already registered by python module 'nim'") != std::string::npos);
    }

    std::filesystem::path forced = dir.write("forced.py", marker_prelude(marker) +
        "import oryx\n"
        "class Forced(oryx.Game, id='nim', overwrite=True):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
        "mark(oryx.describe_game('nim')['origin']['module'])\n");
    python.load(forced);

    CHECK(read_file(marker) == "21;5;forced");
}

TEST_CASE("a script cannot silently replace a C++ game")
{
    TempDir dir;
    std::filesystem::path script = dir.write("clash.py",
        "import oryx\n"
        "class Impostor(oryx.Game, id='tictactoe'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n");

    RunningPython python;
    try
    {
        python.load(script);
        FAIL("the clash should have thrown");
    }
    catch (const ScriptError& error)
    {
        CHECK(std::string(error.traceback()).find("the game 'tictactoe' is already registered by C++") != std::string::npos);
    }
    CHECK(create_game("tictactoe")->name() == "TicTacToe");
}

TEST_CASE("a Python strategy returning an illegal action is an IllegalActionError, not a corrupted game")
{
    std::string output = run_script(
        "import oryx\n"
        "class Cheat(oryx.Strategy, id='cheat'):\n"
        "    def decide(self, context): return 99\n"
        "match = oryx.Match('tictactoe', ['cheat', 'first-legal'])\n"
        "try:\n"
        "    match.play()\n"
        "except oryx.IllegalActionError as e:\n"
        "    mark(str(e) + '|' + str(match.history()))\n");

    CHECK(output == "strategy.decide() returned an illegal action: action 99 is not legal in this state|[]");
}

TEST_CASE("interpreter-control exceptions from a script reach the Python caller untouched")
{
    std::string output = run_script(
        "import oryx\n"
        "class Stop(oryx.Strategy, id='stop'):\n"
        "    raised = None\n"
        "    def decide(self, context): raise Stop.raised\n"
        "class Refuses(oryx.Strategy, id='refuses'):\n"
        "    def __init__(self): raise SystemExit(2)\n"
        "    def decide(self, context): return 0\n"
        "def attempt(kind, call):\n"
        "    try:\n"
        "        call()\n"
        "    except kind:\n"
        "        mark(kind.__name__ + ';')\n"
        "    except oryx.OryxError as e:\n"
        "        mark('wrapped ' + kind.__name__ + ';')\n"
        "for kind in (KeyboardInterrupt, SystemExit, GeneratorExit, MemoryError, RecursionError, ValueError):\n"
        "    Stop.raised = kind\n"
        "    attempt(kind, lambda: oryx.Match('tictactoe', ['stop', 'first-legal']).play())\n"
        "attempt(SystemExit, lambda: oryx.make_strategy('refuses'))\n");

    CHECK(output == "KeyboardInterrupt;SystemExit;GeneratorExit;MemoryError;RecursionError;wrapped ValueError;SystemExit;");
}

TEST_CASE("the state lent to decide() cannot be used after decide() returns")
{
    std::string output = run_script(
        "import oryx\n"
        "class Keeper(oryx.Strategy, id='keeper'):\n"
        "    saved = None\n"
        "    def decide(self, context):\n"
        "        if self.saved is not None:\n"
        "            try:\n"
        "                self.saved.legal_actions()\n"
        "            except oryx.OryxError as e:\n"
        "                mark('stale state;')\n"
        "        self.saved = context.state\n"
        "        return context.state.legal_actions()[0]\n"
        "oryx.Match('tictactoe', ['keeper', 'first-legal']).play()\n"
        "mark(str(oryx.Match('tictactoe', ['first-legal', 'first-legal']).state().legal_actions()))\n");

    CHECK(output.find("stale state;") == 0);
    CHECK(output.find("[0, 1, 2, 3, 4, 5, 6, 7, 8]") != std::string::npos);
}

TEST_CASE("the lent context exposes the game's action features")
{
    std::string output = run_script(
        "import oryx\n"
        "class Decoder(oryx.Strategy, id='decoder'):\n"
        "    def decide(self, context):\n"
        "        mark(str(context.action_features.decode(5)) + ';')\n"
        "        return context.state.legal_actions()[0]\n"
        "oryx.Match('tictactoe', ['decoder', 'first-legal']).apply(0)\n"
        "match = oryx.Match('tictactoe', ['decoder', 'first-legal'])\n"
        "match.decide()\n");

    CHECK(output == "[1, 2];");
}

TEST_CASE("action features stashed by a strategy cannot be used after decide() returns")
{
    std::string output = run_script(
        "import oryx\n"
        "stash = {}\n"
        "class Keeper(oryx.Strategy, id='keeper'):\n"
        "    def decide(self, context):\n"
        "        stash['features'] = context.action_features\n"
        "        mark(str(stash['features'].decode(5)) + ';')\n"
        "        return context.state.legal_actions()[0]\n"
        "match = oryx.Match('tictactoe', ['keeper', 'first-legal'])\n"
        "match.decide()\n"
        "try:\n"
        "    stash['features'].decode(5)\n"
        "except oryx.OryxError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "[1, 2];this action features is no longer valid: it was only lent to the strategy for the duration of decide()");
}

TEST_CASE("a Python strategy returning the invalid-action sentinel is a ScriptError")
{
    std::string output = run_script(
        "import oryx\n"
        "class Sentinel(oryx.Strategy, id='sentinel'):\n"
        "    def decide(self, context): return 4294967295\n"
        "match = oryx.Match('tictactoe', ['sentinel', 'first-legal'])\n"
        "try:\n"
        "    match.play()\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "strategy.decide() returned an illegal action: action 4294967295 is not legal in this state");
}

TEST_CASE("a typed field without a class value is a required parameter that make_game enforces")
{
    std::string output = run_script(
        "import oryx\n"
        "class Needy(oryx.Game, id='needy'):\n"
        "    stones: int\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
        "mark(str(oryx.describe_game('needy')['params'][0]['required']) + '|')\n"
        "try:\n"
        "    oryx.make_game('needy')\n"
        "except oryx.ParamError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "True|'needy': parameter 'stones' is required");
    CHECK(GameRegistry::has("needy") == false);
}

TEST_CASE("script entries are removed when the runtime stops, and come back on the next start")
{
    TempDir dir;
    std::filesystem::path script = dir.write("nim.py", kNim);

    {
        RunningPython python;
        python.load(script);
        CHECK(GameRegistry::has("nim"));
        UniquePtr<IGame> game = create_game("nim");
        REQUIRE(game != nullptr);
        CHECK(game->name() == "Nim");
    }
    CHECK_FALSE(GameRegistry::has("nim"));

    {
        RunningPython python;
        python.load(script);
        CHECK(GameRegistry::has("nim"));
    }
    CHECK_FALSE(GameRegistry::has("nim"));
}

TEST_CASE("the GIL stays held while any participant is a Python script")
{
    StrategyRegistry::register_factory("test/gil-probe", [](const Params&) -> UniquePtr<IStrategy> { return create_unique<GilProbeStrategy>(); });

    g_probe_saw_gil = false;
    run_script(with_nim("oryx.simulate('nim', ['test/gil-probe', 'first-legal'], games=1)\n"));

    StrategyRegistry::unregister_factory("test/gil-probe");
    CHECK(g_probe_saw_gil);
}

TEST_CASE("Match fills every seat with one strategy for a C++ game")
{
    std::string output = run_script(
        "import oryx\n"
        "mark(str(oryx.Match('tictactoe', oryx.make_strategy('first-legal')).play()))\n");

    CHECK(output == "[1.0, -1.0]");
}

#endif
