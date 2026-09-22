#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

#include <cstdlib>
#include <filesystem>
#include <string>

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

int run_python(const std::string& code)
{
    // Resolves against the subprocess's own PATH, not this process's interpreter. Under `uv run
    // build test`/`build build all` (the only supported way to run this suite - CLAUDE.md), `uv
    // run` puts the venv's python first on PATH, so this finds the interpreter the `.pth` file
    // (step 2) was written into. It would resolve elsewhere if this binary were run directly
    // outside `uv run`.
    std::string command = "python -c \"" + code + "\"";
    return std::system(command.c_str());
}

std::filesystem::path extension_path()
{
    return std::filesystem::path(OX_BUILD_OUTPUT_DIR) / "OryxPython";
}

} // namespace

TEST_SUITE("integration")
{

TEST_CASE("research host: import oryx works standalone")
{
    if (!std::filesystem::exists(extension_path()))
    {
        MESSAGE("skipping: research-host extension not built at ", extension_path().string());
        return;
    }
    CHECK(run_python("import oryx; oryx.init()") == 0);
}

TEST_CASE("research host: init() refuses to run inside an embedding host")
{
    // The embedded-host half of the guard: Tests links Oryx the same way Oasis does
    // (linkOryxWholeArchive), so PythonRuntime::start() marks this an embedding host too, and
    // oryx.init() must refuse exactly as it would inside Oasis.
    std::string output = run_oryx_script(
        "try:\n"
        "    oryx.init()\n"
        "    mark('not raised')\n"
        "except oryx.errors.OryxError as e:\n"
        "    mark('raised: ' + str(e))\n");

    CHECK(output.find("raised: ") == 0);
    CHECK(output.find("must not be called inside an embedding host") != std::string::npos);
}

TEST_CASE("research host: assertion hook raises instead of trapping")
{
    if (!std::filesystem::exists(extension_path()))
    {
        return;
    }
    int status = run_python(
        "import oryx, sys; oryx.init();\n"
        "try:\n"
        "    oryx.debug.check(False, 'x')\n"
        "except oryx.errors.OryxAssertionError:\n"
        "    sys.exit(0)\n"
        "sys.exit(1)");
    CHECK(status == 0);
}

TEST_CASE("research host: interruptible batch raises when the interpreter is interrupted mid-batch")
{
    if (!std::filesystem::exists(extension_path()))
    {
        return;
    }
    // Oryx-only scoping (step 1) means the standalone host has no compiled-in game to simulate -
    // Oryx core registers only strategies, and TicTacToe is Oasis-only. The only game reachable
    // from a bare `import oryx` is a *scripted* one, so this uses the existing Nim example
    // script instead of an all-C++ game. That makes the game itself scripted, which per D27
    // wraps an interrupted callback's exception in oryx.errors.ScriptError rather than letting a
    // raw KeyboardInterrupt propagate - still confirms the process doesn't hang forever on a
    // huge batch, which is the property this test cares about (the merge()/run_interruptible()
    // chunking math itself is covered against an all-C++ game in test_python_simulation.cpp,
    // where TicTacToe is available inside the embedded Tests binary).
    std::filesystem::path scripts_dir = repo_file("Oasis/scripts");
    std::string code =
        "import sys; sys.path.insert(0, r'" + scripts_dir.string() + "')\n"
        "import oryx, nim, signal, threading, _thread\n"
        "oryx.init()\n"
        "signal.signal(signal.SIGINT, signal.default_int_handler)\n"
        "threading.Timer(0.05, _thread.interrupt_main).start()\n"
        "try:\n"
        "    oryx.simulate('nim', ['random', 'random'], games=2_000_000)\n"
        "except (KeyboardInterrupt, oryx.errors.ScriptError):\n"
        "    sys.exit(0)\n"
        "sys.exit(1)";
    int status = run_python(code);
    CHECK(status == 0);
}

} // TEST_SUITE("integration")

#endif // OX_ENABLE_PYTHON
