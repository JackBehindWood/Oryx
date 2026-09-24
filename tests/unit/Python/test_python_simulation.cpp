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
