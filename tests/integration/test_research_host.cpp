#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

std::string expected_version()
{
    return std::to_string(VERSION_MAJOR) + "." + std::to_string(VERSION_MINOR) + "." + std::to_string(VERSION_PATCH);
}

} // namespace

TEST_SUITE("integration")
{

TEST_CASE("research host: import oryx works standalone")
{
    if (!can_run_python_extension())
    {
        return;
    }
    CHECK(run_python("import oryx; oryx.init()") == 0);
}

TEST_CASE("research host: the embedded host has __version__ but no init()")
{
    std::string output = run_oryx_script("mark(str(hasattr(oryx, 'init')) + ' ' + oryx.__version__)\n");

    CHECK(output == "False " + expected_version());
}

TEST_CASE("research host: oryx.__version__ matches the C++ version")
{
    if (!can_run_python_extension())
    {
        return;
    }
    CHECK(run_python("import oryx, sys; sys.exit(0 if oryx.__version__ == '" + expected_version() + "' else 1)") == 0);
}

TEST_CASE("research host: the context cannot be re-created once the atexit teardown ran")
{
    if (!can_run_python_extension())
    {
        return;
    }
    int status = run_python(
        "import atexit, os\n"
        "def late():\n"
        "    import oryx\n"
        "    try:\n"
        "        class Late(oryx.Strategy, id='late'):\n"
        "            def decide(self, context): return 0\n"
        "    except oryx.OryxError as e:\n"
        "        os._exit(0 if 'shutting down' in str(e) else 4)\n"
        "    os._exit(3)\n"
        "atexit.register(late)\n"
        "import oryx\n"
        "oryx.init()\n"
        "class Early(oryx.Strategy, id='early'):\n"
        "    def decide(self, context): return 0\n");
    CHECK(status == 0);
}

TEST_CASE("research host: benchmark(memory=True) counts Oryx's own allocations and balances them")
{
    if (!can_run_python_extension())
    {
        return;
    }
    std::filesystem::path scripts_dir = repo_file("Oasis/scripts");
    int status = run_python(
        "import sys; sys.path.insert(0, r'" + scripts_dir.string() + "')\n"
        "import oryx, nim\n"
        "oryx.init()\n"
        "assert oryx.benchmark.benchmark('nim', ['random', 'random'], games=5).memory is None\n"
        "m = oryx.benchmark.benchmark('nim', ['random', 'random'], games=20, memory=True).memory\n"
        "sys.exit(0 if m.allocation_count > 0 and m.allocation_count == m.deallocation_count and m.live_bytes == 0 else 1)");
    CHECK(status == 0);
}

TEST_CASE("research host: init() loads the scripts of the nearest oryx.yaml, an explicit settings file, or nothing")
{
    if (!can_run_python_extension())
    {
        return;
    }
    TempDir dir;
    dir.write("project/scripts/pile.py", "import oryx\nclass Pile(oryx.Game, id='pile'):\n    num_players = 2\n    def new_initial_state(self): return None\n");
    dir.write("project/oryx.yaml", "scripting:\n  roots:\n    - scripts\n");
    std::filesystem::create_directories(dir.path() / "project" / "notebooks");
    std::filesystem::create_directories(dir.path() / "elsewhere");

    std::string nested = (dir.path() / "project" / "notebooks").string();
    std::string elsewhere = (dir.path() / "elsewhere").string();
    std::string settings = (dir.path() / "project" / "oryx.yaml").string();

    CHECK(run_python("import os, sys; os.chdir(r'" + nested + "'); import oryx; oryx.init(); sys.exit(0 if 'pile' in oryx.list_games() else 1)") == 0);
    CHECK(run_python("import os, sys; os.chdir(r'" + elsewhere + "'); import oryx; oryx.init(); sys.exit(0 if oryx.list_games() == [] else 1)") == 0);
    CHECK(run_python("import os, sys; os.chdir(r'" + elsewhere + "'); import oryx; oryx.init(r'" + settings + "'); sys.exit(0 if 'pile' in oryx.list_games() else 1)") == 0);
    CHECK(run_python("import sys, oryx\ntry:\n    oryx.init('/no/such/oryx.yaml')\nexcept oryx.SettingsError:\n    sys.exit(0)\nsys.exit(1)") == 0);
}

TEST_CASE("research host: a scripted game and simulate() exit cleanly under the debug allocator")
{
    if (!can_run_python_extension())
    {
        return;
    }
    std::filesystem::path scripts_dir = repo_file("Oasis/scripts");
    int status = run_python(
        "import sys; sys.path.insert(0, r'" + scripts_dir.string() + "')\n"
        "import oryx, nim\n"
        "oryx.init()\n"
        "result = oryx.simulate('nim', ['random', 'random'], games=20)\n"
        "sys.exit(0 if result.matches == 20 else 1)",
        "PYTHONMALLOC=debug ");
    CHECK(status == 0);
}

TEST_CASE("research host: assertion hook raises instead of trapping")
{
    if (!can_run_python_extension())
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
    if (!can_run_python_extension())
    {
        return;
    }
    // Bare `import oryx` registers no C++ game, so this interrupts a scripted one: the interrupt must pass through its callback untouched.
    std::filesystem::path scripts_dir = repo_file("Oasis/scripts");
    std::string code =
        "import sys; sys.path.insert(0, r'" + scripts_dir.string() + "')\n"
        "import oryx, nim, signal, threading, _thread\n"
        "oryx.init()\n"
        "signal.signal(signal.SIGINT, signal.default_int_handler)\n"
        "threading.Timer(0.05, _thread.interrupt_main).start()\n"
        "try:\n"
        "    oryx.simulate('nim', ['random', 'random'], games=2_000_000)\n"
        "except KeyboardInterrupt:\n"
        "    sys.exit(0)\n"
        "sys.exit(1)";
    int status = run_python(code);
    CHECK(status == 0);
}

} // TEST_SUITE("integration")

#endif // OX_ENABLE_PYTHON
