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
    CHECK(runtime->file_patterns() == std::vector<std::string>{ "*.oryx.py" });
}

TEST_CASE("PythonRuntime runs a script file")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("hello.oryx.py", append_marker(marker, "ran"));

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
    std::filesystem::path script = dir.write("again.oryx.py", append_marker(marker, "x"));

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
    std::filesystem::path script = dir.write("broken.oryx.py", "raise ValueError(\"boom\")\n");

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
        CHECK(error.traceback().find("broken.oryx.py") != std::string::npos);
    }

    runtime->stop();
}

TEST_CASE("PythonRuntime reports a missing script, a missing module and use before start")
{
    TempDir dir;
    UniquePtr<IScriptRuntime> runtime = python_runtime();

    CHECK_THROWS_AS(runtime->load(file_source(dir.path() / "nope.oryx.py")), ScriptError);

    runtime->start();
    CHECK_THROWS_AS(runtime->load(file_source(dir.path() / "nope.oryx.py")), ScriptError);
    CHECK_THROWS_AS(runtime->load(module_source("oryx_no_such_module")), ScriptError);
    runtime->stop();
}

TEST_CASE("PythonRuntime can be stopped and started again")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("twice.oryx.py", append_marker(marker, "y"));

    for (int32_t i = 0; i < 2; ++i)
    {
        UniquePtr<IScriptRuntime> runtime = python_runtime();
        runtime->start();
        runtime->start();
        runtime->load(file_source(script));
        runtime->stop();
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
