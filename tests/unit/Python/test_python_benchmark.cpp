#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("oryx.benchmark.Timer measures a with block")
{
    std::string output = run_oryx_script(
        "import time\n"
        "with oryx.benchmark.Timer() as timer:\n"
        "    time.sleep(0.01)\n"
        "mark(str(timer.elapsed_seconds >= 0.01))\n"
        "manual = oryx.benchmark.Timer()\n"
        "manual.start()\n"
        "manual.stop()\n"
        "mark(str(0.0 <= manual.elapsed_seconds < 1.0))\n");

    CHECK(output == "TrueTrue");
}

TEST_CASE("oryx.benchmark.benchmark reports the same outcome as simulate with timing and throughput")
{
    std::string output = run_oryx_script(
        "b = oryx.benchmark.benchmark('tictactoe', ['random', 'random'], games=30, seed=1)\n"
        "s = oryx.simulate('tictactoe', ['random', 'random'], games=30, seed=1)\n"
        "mark(str(b.outcome.wins == s.wins and b.outcome.draws == s.draws and b.outcome.decisions == s.decisions) + '|')\n"
        "mark(str(b.elapsed_seconds > 0) + str(abs(b.matches_per_second * b.elapsed_seconds - 30) < 1e-6) + str(b.decisions_per_second > 0) + '|')\n"
        "mark(str(b.memory) + str(b.outcome.metadata['seed']) + '|' + repr(b).split()[0])\n");

    CHECK(output == "True|TrueTrueTrue|None1|<oryx.benchmark.BenchmarkResult");
}

TEST_CASE("oryx.benchmark.benchmark counts allocations only when asked and rejects a negative count")
{
    std::string output = run_oryx_script(
        "m = oryx.benchmark.benchmark('tictactoe', ['random', 'random'], games=10, seed=1, memory=True).memory\n"
        "mark(str(isinstance(m, oryx.benchmark.MemoryStats)) + str(m.allocation_count >= 0) + str(m.peak_live_bytes >= 0) + '|')\n"
        "try:\n"
        "    oryx.benchmark.benchmark('tictactoe', ['random', 'random'], games=-1)\n"
        "except oryx.OryxError as e:\n"
        "    mark(str(e))\n");

    CHECK(output == "TrueTrueTrue|the number of matches cannot be negative");
}

#endif
