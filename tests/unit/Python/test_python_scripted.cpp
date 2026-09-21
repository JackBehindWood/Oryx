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

std::string run_script(const std::string& body, const std::string& file_name = "nim.oryx.py")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write(file_name, marker_prelude(marker) + body);

    RunningPython python;
    python.load(script);
    return read_file(marker);
}

std::filesystem::path repo_file(const std::string& relative)
{
    for (std::filesystem::path dir = std::filesystem::current_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
    {
        if (std::filesystem::exists(dir / relative))
        {
            return dir / relative;
        }
    }
    throw std::runtime_error("cannot find " + relative + " above the working directory");
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

TEST_CASE("a class deriving from oryx.Game registers with a schema built from its typed fields")
{
    std::string output = run_script(with_nim(
        "info = oryx.describe_game('nim')\n"
        "mark(info['description'] + '|' + str(info['params']) + '|')\n"
        "origin = info['origin']\n"
        "mark(origin['language'] + '|' + origin['module'] + '|' + str(origin['source_file'].endswith('nim.oryx.py')))\n"));

    CHECK(output ==
          "Take turns removing stones; the last taker wins.|"
          "[{'name': 'stones', 'type': 'int', 'description': '', 'default': 21}, {'name': 'max_take', 'type': 'int', 'description': '', 'default': 3}]|"
          "python|oryx_script_nim|True");
}

TEST_CASE("make_game builds a Python game from keyword parameters and validates them")
{
    std::string output = run_script(with_nim(
        "game = oryx.make_game('nim', stones=15, max_take=4)\n"
        "state = game.new_initial_state()\n"
        "mark(game.name() + '|' + str(game.num_players()) + '|' + str(state.legal_actions()) + '|')\n"
        "state.apply(4)\n"
        "mark(str(state.legal_actions()) + str(state.current_player()) + state.action_to_string(2) + '|')\n"
        "mark(str(oryx.make_game('nim').new_initial_state().legal_actions()) + '|')\n"
        "for call in (lambda: oryx.make_game('nim', stones='x'), lambda: oryx.make_game('nim', pile=3)):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.ParamError as e:\n"
        "        mark(e.key + ';')\n"));

    CHECK(output == "Nim|2|[1, 2, 3, 4]|[1, 2, 3, 4]1take 2|[1, 2, 3]|stones;pile;");
}

TEST_CASE("a Python game plays against C++ strategies through Match and simulate")
{
    std::string output = run_script(with_nim(
        "match = oryx.Match('nim', ['first-legal', 'first-legal'])\n"
        "mark(str(match.play()) + str(len(match.history())) + '|')\n"
        "result = oryx.simulate('nim', ['random', 'random'], games=40, seed=3)\n"
        "mark(f'{result.matches}|{sum(result.wins)}|{result.draws}|')\n"
        "result = oryx.simulate(Nim(), ['first-legal', 'first-legal'], games=2)\n"
        "mark(str(result.wins))\n"));

    CHECK(output == "[1.0, -1.0]21|40|40|0|[2, 0]");
}

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
    std::filesystem::path check = dir.write("check.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "nim = oryx.simulate('nim', ['monte-carlo', 'first-legal'], games=10)\n"
        "tictactoe = oryx.simulate('tictactoe', ['monte-carlo', 'first-legal'], games=10)\n"
        "mark(f'{nim.wins[0]}|{tictactoe.wins[0]}|' + oryx.describe_game('nim')['origin']['source_file'])\n");

    RunningPython python;
    python.load(repo_file("Oasis/scripts/nim.oryx.py"));
    python.load(repo_file("Oasis/scripts/monte_carlo.oryx.py"));
    python.load(check);

    std::string output = read_file(marker);
    size_t first = output.find('|');
    size_t second = output.find('|', first + 1);
    REQUIRE(second != std::string::npos);
    CHECK(std::stoi(output.substr(0, first)) >= 8);
    CHECK(std::stoi(output.substr(first + 1, second - first - 1)) >= 8);
    CHECK(output.find("Oasis/scripts/nim.oryx.py") != std::string::npos);
}

TEST_CASE("a strategy's parameters are set before __init__ runs and simulate seeds strategies that declare seed")
{
    std::string output = run_script(with_nim(std::string(kMonteCarlo) +
        "a = oryx.simulate('nim', ['monte-carlo', 'random'], games=6, seed=4)\n"
        "b = oryx.simulate('nim', ['monte-carlo', 'random'], games=6, seed=4)\n"
        "mark(str(a.wins == b.wins) + '|' + str(a.decisions == b.decisions) + '|')\n"
        "mark(str(oryx.describe_strategy('monte-carlo')['params']))\n"));

    CHECK(output ==
          "True|True|"
          "[{'name': 'playouts', 'type': 'int', 'description': '', 'default': 30}, {'name': 'seed', 'type': 'int', 'description': '', 'default': 1}]");
}

TEST_CASE("re-running the same script replaces its entries; another origin needs overwrite=True")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::string report = "mark(str(oryx.describe_game('nim')['params'][0]['default']) + ';')\n";
    std::filesystem::path script = dir.write("nim.oryx.py", marker_prelude(marker) + kNim + report);

    RunningPython python;
    python.load(script);
    CHECK(read_file(marker) == "21;");

    std::string edited = kNim;
    edited.replace(edited.find("stones: int = 21"), std::string("stones: int = 21").size(), "stones: int = 5");
    dir.write("nim.oryx.py", marker_prelude(marker) + edited + report);
    python.load(script);
    CHECK(read_file(marker) == "21;5;");
    std::vector<std::string> names = GameRegistry::names();
    CHECK(std::count(names.begin(), names.end(), "nim") == 1);

    std::filesystem::path rival = dir.write("rival.oryx.py",
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
        CHECK(error.traceback().find("the game 'nim' is already registered by python module 'oryx_script_nim'") != std::string::npos);
    }

    std::filesystem::path forced = dir.write("forced.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "class Forced(oryx.Game, id='nim', overwrite=True):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
        "mark(oryx.describe_game('nim')['origin']['module'])\n");
    python.load(forced);

    CHECK(read_file(marker) == "21;5;oryx_script_forced");
}

TEST_CASE("a script cannot silently replace a C++ game")
{
    TempDir dir;
    std::filesystem::path script = dir.write("clash.oryx.py",
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

TEST_CASE("bad class definitions fail at import with a message naming the problem")
{
    std::string output = run_script(
        "import oryx, typing\n"
        "def attempt(source):\n"
        "    try:\n"
        "        exec(source, {'oryx': oryx, 'typing': typing})\n"
        "    except oryx.ScriptError as e:\n"
        "        mark(str(e).split(':')[0] + ';')\n"
        "attempt('class A(oryx.Game, id=\"a\"): pass')\n"
        "attempt('class B(oryx.Strategy, id=\"b\"): pass')\n"
        "attempt('class C(oryx.Game, id=\"c\", bogus=1):\\n    num_players = 2\\n    def new_initial_state(self): pass')\n"
        "attempt('class D(oryx.Game, id=\"d\"):\\n    history: list = []\\n    def new_initial_state(self): pass')\n"
        "attempt('class E(oryx.Game, id=\"e\"):\\n    num_players: int = 2\\n    def new_initial_state(self): pass')\n"
        "attempt('class F(oryx.Game, id=\"f\"):\\n    stones: int = \"many\"\\n    def new_initial_state(self): pass')\n"
        "class Fine(oryx.Game, id='fine'):\n"
        "    label: typing.ClassVar[str] = 'x'\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): pass\n"
        "mark(str(oryx.describe_game('fine')['params']))\n");

    CHECK(output ==
          "A must define new_initial_state() to be registered as the game 'a';"
          "B must define decide() to be registered as the strategy 'b';"
          "class C;"
          "D.history is annotated with an unsupported type (fields are parameters of type bool, int, float or str; use typing.ClassVar for anything else);"
          "E.num_players is reserved and cannot be a parameter; assign it without an annotation;"
          "F.stones is annotated as int but its default has another type;"
          "[]");
}

TEST_CASE("a Python game without num_players is rejected when it is created")
{
    std::string output = run_script(
        "import oryx\n"
        "class NoPlayers(oryx.Game, id='no-players'):\n"
        "    def new_initial_state(self): pass\n"
        "try:\n"
        "    oryx.make_game('no-players')\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e))\n");

    CHECK(output.find("NoPlayers must define num_players") != std::string::npos);
}

TEST_CASE("an exception in a Python method surfaces as ScriptError with its traceback and does not end the process")
{
    std::string output = run_script(
        "import oryx\n"
        "class BoomState:\n"
        "    def legal_actions(self): return [0]\n"
        "    def apply(self, action): raise RuntimeError('kaboom')\n"
        "    def undo(self, action): pass\n"
        "    def current_player(self): return 0\n"
        "    def is_terminal(self): return False\n"
        "    def outcome(self): return [0.0, 0.0]\n"
        "    def action_to_string(self, action): return 'x'\n"
        "class Boom(oryx.Game, id='boom'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return BoomState()\n"
        "try:\n"
        "    oryx.simulate('boom', ['first-legal', 'first-legal'], games=1)\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e).split('\\n')[0] + '|' + str('RuntimeError: kaboom' in e.detail))\n");

    CHECK(output == "state.apply(): RuntimeError: kaboom|True");
}

TEST_CASE("a Python strategy returning an illegal action is a ScriptError, not a corrupted game")
{
    std::string output = run_script(
        "import oryx\n"
        "class Cheat(oryx.Strategy, id='cheat'):\n"
        "    def decide(self, context): return 99\n"
        "match = oryx.Match('tictactoe', ['cheat', 'first-legal'])\n"
        "try:\n"
        "    match.play()\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e) + '|' + str(match.history()))\n");

    CHECK(output == "strategy.decide() returned an illegal action: action 99 is not legal in this state|[]");
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

TEST_CASE("oryx.State supplies a default action_to_string")
{
    std::string output = run_script(
        "import oryx\n"
        "class Counter(oryx.State):\n"
        "    pass\n"
        "mark(Counter().action_to_string(3) + '|' + str(isinstance(Counter(), oryx.State)))\n");

    CHECK(output == "3|True");
}

TEST_CASE("register_game and register_strategy register factory functions with parameters")
{
    std::string output = run_script(with_nim(
        "class Small(Nim):\n"
        "    pass\n"
        "def make_small(stones, label):\n"
        "    game = Small()\n"
        "    game.stones = stones\n"
        "    return game\n"
        "oryx.register_game('nim-small', make_small, params={'stones': 7, 'label': str}, description='Small nim')\n"
        "info = oryx.describe_game('nim-small')\n"
        "mark(info['description'] + str(info['params']) + info['origin']['module'] + '|')\n"
        "game = oryx.make_game('nim-small', label='x', stones=5)\n"
        "mark(str(game.new_initial_state().legal_actions()) + '|')\n"
        "try:\n"
        "    oryx.make_game('nim-small')\n"
        "except oryx.ParamError as e:\n"
        "    mark(str(e))\n"));

    CHECK(output ==
          "Small nim[{'name': 'stones', 'type': 'int', 'description': '', 'default': 7}, {'name': 'label', 'type': 'string', 'description': ''}]oryx_script_nim|"
          "[1, 2, 3]|"
          "'nim-small': parameter 'label' is required");
}

TEST_CASE("script entries are removed when the runtime stops, and come back on the next start")
{
    TempDir dir;
    std::filesystem::path script = dir.write("nim.oryx.py", kNim);

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

#endif
