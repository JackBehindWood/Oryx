#include "doctest.h"

#include "ScriptingTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

using RuntimeList = std::vector<UniquePtr<IScriptRuntime>>;

RuntimeList two_runtimes(std::vector<std::string>& log, bool fail_alpha_start = false)
{
    RuntimeList runtimes;
    runtimes.push_back(create_unique<FakeRuntime>("alpha", "*.alpha.txt", log, fail_alpha_start));
    runtimes.push_back(create_unique<FakeRuntime>("beta", "*.beta.txt", log));
    return runtimes;
}

RuntimeList one_runtime(std::vector<std::string>& log)
{
    RuntimeList runtimes;
    runtimes.push_back(create_unique<FakeRuntime>("alpha", "*.alpha.txt", log));
    return runtimes;
}

ScriptDiscoveryOptions options_for(const TempDir& dir)
{
    ScriptDiscoveryOptions options;
    options.root = dir.path();
    return options;
}

} // namespace

TEST_CASE("ScriptingLayer starts the runtimes that have sources, loads in discovery order, and stops in reverse order")
{
    TempDir dir;
    dir.write("a.alpha.txt");
    dir.write("b.alpha.txt");
    dir.write("c.beta.txt");
    std::vector<std::string> log;

    {
        LayerStack stack;
        stack.push_layer<ScriptingLayer>(options_for(dir), two_runtimes(log));

        CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:a.alpha.txt", "alpha:load:b.alpha.txt",
                                               "beta:start", "beta:load:c.beta.txt" });
        log.clear();
    }

    CHECK(log == std::vector<std::string>{ "beta:stop", "alpha:stop" });
}

TEST_CASE("ScriptingLayer never starts a runtime that has no sources")
{
    TempDir dir;
    dir.write("only.alpha.txt");
    std::vector<std::string> log;

    {
        LayerStack stack;
        stack.push_layer<ScriptingLayer>(options_for(dir), two_runtimes(log));
        CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:only.alpha.txt" });
        log.clear();
    }

    CHECK(log == std::vector<std::string>{ "alpha:stop" });
}

TEST_CASE("ScriptingLayer logs a script's error and keeps loading the remaining sources")
{
    TempDir dir;
    dir.write("a.alpha.txt");
    dir.write("b-broken.alpha.txt");
    dir.write("c.alpha.txt");
    std::vector<std::string> log;

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), one_runtime(log));

    CHECK_FALSE(layer.is_disabled());
    CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:a.alpha.txt", "alpha:load:b-broken.alpha.txt", "alpha:load:c.alpha.txt" });
}

TEST_CASE("ScriptingLayer skips an explicit file no runtime claims")
{
    TempDir dir;
    dir.write("thing.lua");
    std::vector<std::string> log;

    ScriptDiscoveryOptions options = options_for(dir);
    options.script_files = { "thing.lua" };

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options, one_runtime(log));

    CHECK_FALSE(layer.is_disabled());
    CHECK(log.empty());
}

TEST_CASE("ScriptingLayer routes a --module to the only registered runtime")
{
    TempDir dir;
    std::vector<std::string> log;

    ScriptDiscoveryOptions options = options_for(dir);
    options.modules = { "my_game" };

    LayerStack stack;
    stack.push_layer<ScriptingLayer>(options, one_runtime(log));

    CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:my_game" });
}

TEST_CASE("ScriptingLayer does not guess a runtime for a --module when several are registered")
{
    TempDir dir;
    std::vector<std::string> log;

    ScriptDiscoveryOptions options = options_for(dir);
    options.modules = { "my_game" };

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options, two_runtimes(log));

    CHECK_FALSE(layer.is_disabled());
    CHECK(log.empty());
}

TEST_CASE("ScriptingLayer is disabled by the layer boundary when a runtime fails to start")
{
    TempDir dir;
    dir.write("a.alpha.txt");
    std::vector<std::string> log;

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), two_runtimes(log, /*fail_alpha_start=*/true));

    CHECK(layer.is_disabled());
    CHECK(log == std::vector<std::string>{ "alpha:start" });
    CHECK_NOTHROW(stack.update());
}

TEST_CASE("ScriptingLayer with no runtimes attaches harmlessly")
{
    TempDir dir;
    dir.write("a.alpha.txt");

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), RuntimeList{});

    CHECK_FALSE(layer.is_disabled());
}
