#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("oryx.log routes each level to the client logger without treating the text as a format string")
{
    TempDir dir;
    std::filesystem::path script = dir.write("log.oryx.py",
        "import oryx\n"
        "oryx.log.trace('t')\n"
        "oryx.log.info('i')\n"
        "oryx.log.warn('w')\n"
        "oryx.log.error('e')\n"
        "oryx.log.critical('{} {0}')\n");

    ClientLogCapture capture;
    RunningPython python;
    python.load(script);

    CHECK(capture.lines() == std::vector<std::string>{ "trace|t", "info|i", "warning|w", "error|e", "critical|{} {0}" });
}

TEST_CASE("oryx.log.install bridges the standard logging module to the client logger")
{
    TempDir dir;
    std::filesystem::path script = dir.write("bridge.oryx.py",
        "import logging, oryx\n"
        "oryx.log.install()\n"
        "game = logging.getLogger('game')\n"
        "game.setLevel(logging.DEBUG)\n"
        "game.debug('d')\n"
        "game.info('i')\n"
        "game.warning('w')\n"
        "game.error('e')\n"
        "game.critical('c')\n");

    ClientLogCapture capture;
    RunningPython python;
    python.load(script);

    CHECK(capture.lines() == std::vector<std::string>{ "trace|game: d", "info|game: i", "warning|game: w", "error|game: e", "critical|game: c" });
}

TEST_CASE("oryx.assertions.check passes silently and raises OryxAssertionError after logging when it fails")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("check.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "oryx.assertions.check(True, 'fine')\n"
        "try:\n"
        "    oryx.assertions.check(False, 'nope')\n"
        "except oryx.OryxAssertionError as e:\n"
        "    mark(str(e))\n"
        "    assert isinstance(e, oryx.OryxError)\n");

    ClientLogCapture capture;
    RunningPython python;
    python.load(script);

    CHECK(read_file(marker) == "Assertion failed: nope");
    CHECK(capture.lines() == std::vector<std::string>{ "error|Assertion failed: nope" });
}

TEST_CASE("an uncaught failed check surfaces from load as a ScriptError naming the exception")
{
    TempDir dir;
    std::filesystem::path script = dir.write("uncaught.oryx.py", "import oryx\noryx.assertions.check(False, 'nope')\n");

    ClientLogCapture capture;
    RunningPython python;

    try
    {
        python.load(script);
        FAIL("load should have thrown");
    }
    catch (const ScriptError& error)
    {
        CHECK(error.traceback().find("OryxAssertionError") != std::string::npos);
        CHECK(error.traceback().find("nope") != std::string::npos);
    }
}

TEST_CASE("oryx.log and oryx.assertions raise OryxError before Oryx is initialised")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("guard.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "for call in (lambda: oryx.log.info('x'), lambda: oryx.assertions.check(True)):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.OryxError as e:\n"
        "        mark(str(e) + '|' + repr(e.detail) + ';')\n");

    RunningPython python;
    {
        UninitialisedScope uninitialised;
        python.load(script);
    }

    CHECK(read_file(marker) ==
          "oryx.log.info() was called before Oryx was initialised|'';"
          "oryx.assertions.check() was called before Oryx was initialised|'';");
}

TEST_CASE("the Python exception types mirror the C++ error hierarchy")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("hierarchy.oryx.py", marker_prelude(marker) +
        "import oryx\n"
        "for kind in (oryx.ParamError, oryx.ScriptError, oryx.OryxAssertionError):\n"
        "    assert issubclass(kind, oryx.OryxError)\n"
        "assert issubclass(oryx.OryxError, Exception)\n"
        "mark(oryx.OryxError.__module__)\n");

    RunningPython python;
    python.load(script);

    CHECK(read_file(marker) == "oryx");
}

TEST_CASE("the embedded module survives an interpreter restart")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("again.oryx.py", marker_prelude(marker) + "import oryx\nmark('ok;')\n");

    for (int32_t i = 0; i < 2; ++i)
    {
        RunningPython python;
        python.load(script);
    }

    CHECK(read_file(marker) == "ok;ok;");
}

#endif
