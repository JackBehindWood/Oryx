#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

extern "C" int PyGILState_Check(void);

namespace
{

bool g_probe_saw_gil = true;

class GilProbeStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override
    {
        g_probe_saw_gil = PyGILState_Check() != 0;
        return context.state().legal_actions()[0];
    }
};

class ScopedStrategy
{
public:
    ScopedStrategy(std::string name, StrategyRegistry::Factory factory)
        : m_name(std::move(name))
    {
        StrategyRegistry::register_factory(m_name, std::move(factory));
    }

    ~ScopedStrategy() { StrategyRegistry::unregister_factory(m_name); }

    ScopedStrategy(const ScopedStrategy&) = delete;
    ScopedStrategy& operator=(const ScopedStrategy&) = delete;

private:
    std::string m_name;
};

std::string run_script(const std::string& body)
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("script.py", marker_prelude(marker) + "import oryx\n" + body);

    RunningPython python;
    python.load(script);
    return read_file(marker);
}

std::string summary(const BatchResult& result)
{
    return std::to_string(result.matches) + "|[" + std::to_string(result.wins[0]) + ", " + std::to_string(result.wins[1]) + "]|" + std::to_string(result.draws) + "|" + std::to_string(result.decisions);
}

} // namespace

TEST_CASE("oryx.Match steps a game by hand with undo, redo and history")
{
    std::string output = run_script(
        "match = oryx.Match('tictactoe', ['first-legal', 'first-legal'])\n"
        "state = match.state()\n"
        "mark(str(state.legal_actions()) + '|')\n"
        "match.apply(4)\n"
        "mark(str(match.history()) + str(state.current_player()) + str(match.current_player()) + '|')\n"
        "mark(str(match.decide()) + '|')\n"
        "mark(str(match.undo()) + str(match.undo()) + str(match.redo()) + str(match.history()) + '|')\n"
        "mark(state.action_to_string(4))\n");

    CHECK(output == "[0, 1, 2, 3, 4, 5, 6, 7, 8]|[4]11|0|4None4[4]|row 2, col 2");
}

TEST_CASE("the state handle of a match is read-only, so the match history cannot be bypassed")
{
    std::string output = run_script(
        "match = oryx.Match('tictactoe', ['first-legal', 'first-legal'])\n"
        "state = match.state()\n"
        "match.apply(4)\n"
        "for call in (lambda: state.apply(0), lambda: state.undo(4)):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.OryxError as e:\n"
        "        mark(str(e) + ';')\n"
        "mark(str(match.undo()) + str(match.history()) + str(state.legal_actions()))\n");

    CHECK(output ==
          "this state belongs to a match and is read-only: use match.apply() and match.undo();"
          "this state belongs to a match and is read-only: use match.apply() and match.undo();"
          "4[][0, 1, 2, 3, 4, 5, 6, 7, 8]");
}

TEST_CASE("oryx.Match rejects illegal actions and mismatched strategy counts without touching the state")
{
    std::string output = run_script(
        "match = oryx.Match('tictactoe', ['first-legal', 'first-legal'])\n"
        "match.apply(4)\n"
        "for call in (lambda: match.apply(4), lambda: match.apply(99), lambda: oryx.Match('tictactoe', ['random'])):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.OryxError as e:\n"
        "        mark(type(e).__name__ + ':' + str(e) + ';')\n"
        "mark(str(match.history()))\n");

    CHECK(output ==
          "IllegalActionError:action 4 is not legal in this state;"
          "IllegalActionError:action 99 is not legal in this state;"
          "OryxError:TicTacToe has 2 players but 1 strategies were given;"
          "[4]");
}

TEST_CASE("oryx.Match plays to the end and reports the rewards")
{
    std::string output = run_script(
        "match = oryx.Match('tictactoe', ['first-legal', 'first-legal'])\n"
        "mark(str(match.play()) + '|' + str(match.is_terminal()) + '|' + str(match.outcome()) + '|' + str(len(match.history())))\n");

    CHECK(output == "[1.0, -1.0]|True|[1.0, -1.0]|7");
}

TEST_CASE("a state from new_initial_state is owned by Python and independent of the game")
{
    std::string output = run_script(
        "state = oryx.make_game('tictactoe').new_initial_state()\n"
        "state.apply(0)\n"
        "mark(str(state.legal_actions()) + str(state.is_terminal()) + str(state.outcome()))\n");

    CHECK(output == "[1, 2, 3, 4, 5, 6, 7, 8]False[0.0, 0.0]");
}

TEST_CASE("oryx.simulate matches a C++ BatchRunner and is reproducible per seed")
{
    UniquePtr<IGame> game = create_game("tictactoe");
    RandomStrategy first(7);
    RandomStrategy second(8);
    BatchResult expected = BatchRunner(*game, { &first, &second }).run(200);

    std::string output = run_script(
        "def summary(r):\n"
        "    return f'{r.matches}|{r.wins}|{r.draws}|{r.decisions}'\n"
        "a = oryx.simulate('tictactoe', ['random', 'random'], games=200, seed=7)\n"
        "b = oryx.simulate('tictactoe', ['random', 'random'], games=200, seed=7)\n"
        "c = oryx.simulate('tictactoe', ['random', 'random'], games=200, seed=100)\n"
        "mark(summary(a) + ';' + str(summary(a) == summary(b)) + ';' + str(summary(a) == summary(c)))\n");

    CHECK(output == summary(expected) + ";True;False");
}

TEST_CASE("oryx.simulate leaves strategy instances unseeded by the master seed")
{
    std::string output = run_script(
        "mine = oryx.make_strategy('random', seed=5)\n"
        "r = oryx.simulate('tictactoe', [mine, 'first-legal'], games=10, seed=1)\n"
        "mark(str(r.matches) + repr(r))\n");

    CHECK(output.find("10<oryx.BatchResult matches=10 wins=[") == 0);
}

TEST_CASE("oryx.BatchRunner runs repeatedly and rejects a negative count")
{
    std::string output = run_script(
        "runner = oryx.BatchRunner('tictactoe', ['first-legal', 'first-legal'])\n"
        "r = runner.run(3)\n"
        "mark(f'{r.matches}|{r.wins}|{r.draws}|{r.decisions}|{r.rewards};')\n"
        "try:\n"
        "    runner.run(-1)\n"
        "except oryx.OryxError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "3|[3, 0]|0|21|[3.0, -3.0];the number of matches cannot be negative");
}

TEST_CASE("oryx.Random is seedable and matches the C++ generator")
{
    Random expected(5);
    std::string expected_ints = std::to_string(expected.get_int(0, 9)) + "," + std::to_string(expected.get_int(0, 9));

    std::string output = run_script(
        "r = oryx.Random(5)\n"
        "mark(str(r.get_int(0, 9)) + ',' + str(r.get_int(0, 9)) + ';')\n"
        "r.seed(5)\n"
        "mark(str(r.get_int(0, 9)) + ';' + str(0.0 <= r.get_double() < 1.0) + str(r.get_bool(1.0)))\n");

    CHECK(output == expected_ints + ";" + std::to_string(Random(5).get_int(0, 9)) + ";TrueTrue");
}

TEST_CASE("oryx.simulate releases the GIL for an all-C++ batch")
{
    ScopedStrategy probe("test/gil-probe", [](const Params&) -> UniquePtr<IStrategy> { return create_unique<GilProbeStrategy>(); });

    g_probe_saw_gil = true;
    run_script("oryx.simulate('tictactoe', ['test/gil-probe', 'first-legal'], games=1)\n");

    CHECK_FALSE(g_probe_saw_gil);
}

TEST_CASE("a C++ SettingsError reaches Python as oryx.SettingsError")
{
    ScopedStrategy unsettled("test/unsettled", [](const Params&) -> UniquePtr<IStrategy> { throw SettingsError("settings: bad value"); });

    std::string output = run_script(
        "try:\n"
        "    oryx.make_strategy('test/unsettled')\n"
        "except oryx.SettingsError as e:\n"
        "    mark(type(e).__name__ + ':' + str(e))\n");

    CHECK(output == "SettingsError:settings: bad value");
}

TEST_CASE("oryx.simulate is interruptible mid-batch for an all-C++ game")
{
    // The batch runs without the GIL, so only run_interruptible's check between chunks can see the interrupt.
    std::string output = run_script(
        "import signal, threading, _thread\n"
        "signal.signal(signal.SIGINT, signal.default_int_handler)\n"
        "threading.Timer(0.05, _thread.interrupt_main).start()\n"
        "try:\n"
        "    oryx.simulate('tictactoe', ['random', 'random'], games=2_000_000)\n"
        "    mark('not interrupted')\n"
        "except KeyboardInterrupt:\n"
        "    mark('interrupted')\n");

    CHECK(output == "interrupted");
}

#endif
