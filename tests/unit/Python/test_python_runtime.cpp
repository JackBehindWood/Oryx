#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("PythonRuntime registers itself as the 'python' script runtime")
{
    std::vector<std::string> names = ScriptRuntimeRegistry::names();
    CHECK(std::find(names.begin(), names.end(), "python") != names.end());

    UniquePtr<IScriptRuntime> runtime = python_runtime();
    CHECK(runtime->language() == "python");
    CHECK(runtime->file_extensions() == std::vector<std::string>{ ".py" });
}

TEST_CASE("PythonRuntime runs a script file")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("hello.py", append_marker(marker, "ran"));

    UniquePtr<IScriptRuntime> runtime = python_runtime();
    runtime->start();
    runtime->load(file_source(script));
    runtime->stop();

    CHECK(read_file(marker) == "ran");
}

TEST_CASE("PythonRuntime reload runs the script again")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("again.py", append_marker(marker, "x"));

    UniquePtr<IScriptRuntime> runtime = python_runtime();
    runtime->start();
    runtime->load(file_source(script));
    runtime->reload(file_source(script));
    runtime->stop();

    CHECK(read_file(marker) == "xx");
}

TEST_CASE("PythonRuntime imports a module source")
{
    UniquePtr<IScriptRuntime> runtime = python_runtime();
    runtime->start();
    CHECK_NOTHROW(runtime->load(module_source("json")));
    CHECK_NOTHROW(runtime->reload(module_source("json")));
    runtime->stop();
}

TEST_CASE("PythonRuntime turns a script exception into a ScriptError carrying the traceback")
{
    TempDir dir;
    std::filesystem::path script = dir.write("broken.py", "raise ValueError(\"boom\")\n");

    UniquePtr<IScriptRuntime> runtime = python_runtime();
    runtime->start();

    try
    {
        runtime->load(file_source(script));
        FAIL("load should have thrown");
    }
    catch (const ScriptError& error)
    {
        CHECK(std::string(error.what()).find("ValueError: boom") != std::string::npos);
        CHECK(error.traceback().find("broken.py") != std::string::npos);
    }

    runtime->stop();
}

TEST_CASE("PythonRuntime reports a missing script, a missing module and use before start")
{
    TempDir dir;
    UniquePtr<IScriptRuntime> runtime = python_runtime();

    CHECK_THROWS_AS(runtime->load(file_source(dir.path() / "nope.py")), ScriptError);

    runtime->start();
    CHECK_THROWS_AS(runtime->load(file_source(dir.path() / "nope.py")), ScriptError);
    CHECK_THROWS_AS(runtime->load(module_source("oryx_no_such_module")), ScriptError);
    runtime->stop();
}

TEST_CASE("a script that shares its name with a standard-library module is reported as shadowed")
{
    TempDir dir;
    RunningPython python;

    try
    {
        python.load(dir.write("random.py", "x = 1\n"));
        FAIL("the shadowed script should have thrown");
    }
    catch (const ScriptError& error)
    {
        CHECK(std::string(error.what()).find("is shadowed by the module 'random'") != std::string::npos);
        CHECK(std::string(error.what()).find("rename the script") != std::string::npos);
    }
}

TEST_CASE("two roots that both define a script of the same name clash instead of one silently winning")
{
    TempDir first;
    TempDir second;
    RunningPython python;

    python.load(first.write("twin.py", "x = 1\n"));
    CHECK_THROWS_WITH_AS(python.load(second.write("twin.py", "x = 2\n")), doctest::Contains("shadowed by the module 'twin'"), ScriptError);
}

TEST_CASE("a script whose name contains a dot cannot be imported and says so")
{
    TempDir dir;
    RunningPython python;
    CHECK_THROWS_WITH_AS(python.load(dir.write("a.b.py", "x = 1\n")), doctest::Contains("cannot contain a dot"), ScriptError);
}

TEST_CASE("unload forgets a root's modules so the next load runs them again, and drops the root from sys.path")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("again.py", append_marker(marker, "z"));

    UniquePtr<IScriptRuntime> runtime = python_runtime();
    runtime->start();
    runtime->load(file_source(script));
    runtime->load(file_source(script));
    CHECK(read_file(marker) == "z");

    runtime->unload();
    runtime->load(file_source(script));
    CHECK(read_file(marker) == "zz");
    runtime->stop();
}

TEST_CASE("PythonRuntime can be stopped and started again")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("twice.py", append_marker(marker, "y"));

    for (int32_t i = 0; i < 2; ++i)
    {
        UniquePtr<IScriptRuntime> runtime = python_runtime();
        CHECK_FALSE(runtime->running());
        runtime->start();
        runtime->start();
        CHECK(runtime->running());
        runtime->load(file_source(script));
        runtime->stop();
        CHECK_FALSE(runtime->running());
    }

    CHECK(read_file(marker) == "yy");
}

#else

TEST_CASE("Python off: no 'python' script runtime is registered")
{
    std::vector<std::string> names = ScriptRuntimeRegistry::names();
    CHECK(std::find(names.begin(), names.end(), "python") == names.end());
}

#endif
