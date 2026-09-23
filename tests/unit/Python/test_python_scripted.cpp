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

TEST_CASE("a class deriving from oryx.Game registers with a schema built from its typed fields")
{
    std::string output = run_script(with_nim(
        "info = oryx.describe_game('nim')\n"
        "mark(info['description'] + '|' + str(info['params']) + '|')\n"
        "origin = info['origin']\n"
        "mark(origin['language'] + '|' + origin['module'] + '|' + str(origin['source_file'].endswith('nim.py')))\n"));

    CHECK(output ==
          "Take turns removing stones; the last taker wins.|"
          "[{'name': 'stones', 'type': 'int', 'description': '', 'required': False, 'default': 21}, {'name': 'max_take', 'type': 'int', 'description': '', 'required': False, 'default': 3}]|"
          "python|nim|True");
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

TEST_CASE("a strategy's parameters are set before __init__ runs and simulate seeds strategies that declare seed")
{
    std::string output = run_script(with_nim(std::string(kMonteCarlo) +
        "a = oryx.simulate('nim', ['monte-carlo', 'random'], games=6, seed=4)\n"
        "b = oryx.simulate('nim', ['monte-carlo', 'random'], games=6, seed=4)\n"
        "mark(str(a.wins == b.wins) + '|' + str(a.decisions == b.decisions) + '|')\n"
        "mark(str(oryx.describe_strategy('monte-carlo')['params']))\n"));

    CHECK(output ==
          "True|True|"
          "[{'name': 'playouts', 'type': 'int', 'description': '', 'required': False, 'default': 30}, {'name': 'seed', 'type': 'int', 'description': '', 'required': False, 'default': 1}]");
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

TEST_CASE("a state class missing a required method is reported when the state is created, naming the method")
{
    std::string output = run_script(
        "import oryx\n"
        "class Broken(oryx.Game, id='broken'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return BrokenState()\n"
        "class BrokenState:\n"
        "    def apply(self, action): pass\n"
        "try:\n"
        "    oryx.make_game('broken').new_initial_state()\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "state class 'BrokenState' must define legal_actions()");
}

TEST_CASE("a script method returning the wrong type is a ScriptError naming the method and the type")
{
    std::string output = run_script(
        "import oryx\n"
        "class Wrong(oryx.Game, id='wrong'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return WrongState()\n"
        "class WrongState(oryx.State):\n"
        "    def legal_actions(self): return 'nope'\n"
        "    def apply(self, action): pass\n"
        "    def undo(self, action): pass\n"
        "    def current_player(self): return 'zero'\n"
        "    def is_terminal(self): return []\n"
        "    def outcome(self): return [0.0, 0.0]\n"
        "state = oryx.make_game('wrong').new_initial_state()\n"
        "for call in (state.legal_actions, state.current_player, state.is_terminal):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.ScriptError as e:\n"
        "        mark(str(e) + ';')\n");

    CHECK(output ==
          "state.legal_actions(): expected a sequence of ints, got str;"
          "state.current_player(): expected an int, got str;"
          "state.is_terminal(): expected a bool, got list;");
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

TEST_CASE("a game without action features gives the strategy None")
{
    std::string output = run_script(with_nim(
        "class Peek(oryx.Strategy, id='peek'):\n"
        "    def decide(self, context):\n"
        "        mark(str(context.action_features))\n"
        "        return context.state.legal_actions()[0]\n"
        "oryx.Match('nim', ['peek', 'first-legal']).decide()\n"));

    CHECK(output == "None");
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

TEST_CASE("a game whose new_initial_state returns something that is not a state fails with a ScriptError naming it")
{
    std::string output = run_script(
        "import oryx\n"
        "class NoState(oryx.Game, id='no-state'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
        "try:\n"
        "    oryx.make_game('no-state').new_initial_state()\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "game.new_initial_state(): expected a state, got NoneType");
}

TEST_CASE("register_game rejects a factory that is not callable, and a class that gives an empty id")
{
    std::string output = run_script(
        "import oryx\n"
        "for attempt in (lambda: oryx.register_game('bad-factory', 42),\n"
        "                lambda: exec(\"class E(oryx.Game, id=''):\\n    num_players = 2\\n    def new_initial_state(self): pass\", {'oryx': oryx})):\n"
        "    try:\n"
        "        attempt()\n"
        "    except oryx.ScriptError as e:\n"
        "        mark(str(e) + ';')\n");

    CHECK(output ==
          "the factory for the game 'bad-factory' must be callable, got 'int';"
          "class E: id cannot be empty;");
}

TEST_CASE("parameters accept any object that behaves as an int or a float, such as a numpy scalar")
{
    std::string output = run_script(with_nim(
        "class Index:\n"
        "    def __index__(self): return 7\n"
        "class Real:\n"
        "    def __float__(self): return 2.5\n"
        "class Scaled(oryx.Game, id='scaled'):\n"
        "    scale: float = 1.0\n"
        "    stones: int = 3\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
        "game = oryx.make_game('nim', stones=Index())\n"
        "mark(str(len(game.new_initial_state().legal_actions())) + '|')\n"
        "oryx.make_game('scaled', scale=Real(), stones=Index())\n"));

    CHECK(output == "3|");
}

TEST_CASE("string annotations, as produced by `from __future__ import annotations`, still declare parameters")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("lazy.py",
        "from __future__ import annotations\n" + marker_prelude(marker) +
        "import oryx\n"
        "class Lazy(oryx.Game, id='lazy'):\n"
        "    stones: int = 4\n"
        "    label: str\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
        "mark(str([(p['name'], p['type'], p['required']) for p in oryx.describe_game('lazy')['params']]))\n");

    RunningPython python;
    python.load(script);

    CHECK(read_file(marker) == "[('stones', 'int', False), ('label', 'string', True)]");
}

TEST_CASE("legal_actions is asked of the script once per position: apply's legality check reuses it, apply and undo refresh it")
{
    std::string output = run_script(
        "import oryx\n"
        "calls = [0]\n"
        "class Counting(oryx.Game, id='counting'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return CountingState()\n"
        "class CountingState(oryx.State):\n"
        "    def __init__(self): self.stones = 5\n"
        "    def legal_actions(self):\n"
        "        calls[0] += 1\n"
        "        return [1, 2] if self.stones > 1 else [1]\n"
        "    def apply(self, action): self.stones -= action\n"
        "    def undo(self, action): self.stones += action\n"
        "    def current_player(self): return 0\n"
        "    def is_terminal(self): return self.stones == 0\n"
        "    def outcome(self): return [0.0, 0.0]\n"
        "state = oryx.make_game('counting').new_initial_state()\n"
        "state.legal_actions(); state.legal_actions()\n"
        "mark(str(calls[0]))\n"
        "state.apply(2)\n"
        "mark(str(calls[0]))\n"
        "mark(str(state.legal_actions()) + str(calls[0]))\n"
        "state.undo(2)\n"
        "mark(str(state.legal_actions()) + str(calls[0]))\n");

    CHECK(output == "11" "[1, 2]2" "[1, 2]3");
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
          "Small nim[{'name': 'stones', 'type': 'int', 'description': '', 'required': False, 'default': 7}, {'name': 'label', 'type': 'string', 'description': '', 'required': True}]nim|"
          "[1, 2, 3]|"
          "'nim-small': parameter 'label' is required");
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

TEST_CASE("the base classes' placeholder methods raise NotImplementedError and never count as definitions")
{
    std::string output = run_script(
        "import oryx\n"
        "class Lazy(oryx.Strategy):\n"
        "    pass\n"
        "try:\n"
        "    Lazy().decide(None)\n"
        "except NotImplementedError as e:\n"
        "    mark(str(e) + ';')\n"
        "class HalfState(oryx.State):\n"
        "    def apply(self, action): pass\n"
        "class Half(oryx.Game, id='half'):\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return HalfState()\n"
        "try:\n"
        "    oryx.make_game('half').new_initial_state()\n"
        "except oryx.ScriptError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "Lazy must define decide();state class 'HalfState' must define legal_actions()");
}

TEST_CASE("Match, simulate and benchmark accept registered classes, and one strategy fills every seat")
{
    std::string output = run_script(with_nim(std::string(kMonteCarlo) +
        "match = oryx.Match(Nim, [MonteCarlo, 'random'])\n"
        "mark(str(match.state().current_player()) + ';')\n"
        "result = oryx.simulate(Nim, MonteCarlo, games=4, seed=3)\n"
        "mark(str(result.metadata['strategies']) + ';')\n"
        "mark(str(oryx.simulate('nim', ('random', 'first-legal'), games=2).metadata['strategies']) + ';')\n"
        "mark(str(oryx.benchmark.benchmark(Nim, 'random', games=2).outcome.matches) + ';')\n"
        "mark(str(oryx.Match('tictactoe', oryx.make_strategy('first-legal')).play()) + ';')\n"
        "mark(oryx.describe_strategy(MonteCarlo)['name'] + ':' + str(oryx.simulate(Nim, [oryx.make_strategy(MonteCarlo, playouts=2), 'random'], games=2).matches) + ';')\n"
        "class Unregistered(oryx.Strategy):\n"
        "    def decide(self, context): return 1\n"
        "try:\n"
        "    oryx.Match(Nim, Unregistered)\n"
        "except oryx.OryxError as e:\n"
        "    mark(str(e))\n"));

    CHECK(output ==
          "0;"
          "['monte-carlo', 'monte-carlo'];"
          "['random', 'first-legal'];"
          "2;"
          "[1.0, -1.0];"
          "monte-carlo:2;"
          "the class Unregistered is not registered; give it an id (`class Unregistered(..., id=\"...\")`) to pass the class itself");
}

TEST_CASE("a class resolves through its registry entry, so seeding by name applies to it")
{
    std::string output = run_script(with_nim(std::string(kMonteCarlo) +
        "by_class = oryx.simulate(Nim, [MonteCarlo, 'random'], games=6, seed=11)\n"
        "by_name = oryx.simulate('nim', ['monte-carlo', 'random'], games=6, seed=11)\n"
        "mark(str(by_class.wins == by_name.wins and by_class.decisions == by_name.decisions))\n"));

    CHECK(output == "True");
}

#endif
