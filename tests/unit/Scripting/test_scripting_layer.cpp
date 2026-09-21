#include "doctest.h"

#include "ScriptingTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

void add_two_runtimes(FakeRuntimeSet& runtimes, std::vector<std::string>& log, bool fail_alpha_start = false)
{
    runtimes.add("alpha", ".alpha", log, fail_alpha_start);
    runtimes.add("beta", ".beta", log);
}

void add_one_runtime(FakeRuntimeSet& runtimes, std::vector<std::string>& log)
{
    runtimes.add("alpha", ".alpha", log);
}

ScriptDiscoveryOptions options_for(const TempDir& dir)
{
    ScriptDiscoveryOptions options;
    options.root = dir.path();
    options.roots = { dir.path().string() };
    return options;
}

} // namespace

TEST_CASE("ScriptingLayer starts the runtimes that have sources and loads in discovery order; shutdown stops them in reverse start order")
{
    TempDir dir;
    dir.write("a.alpha");
    dir.write("b.alpha");
    dir.write("c.beta");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_two_runtimes(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    {
        LayerStack stack;
        stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());

        CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:a.alpha", "alpha:load:b.alpha",
                                               "beta:start", "beta:load:c.beta" });
        log.clear();
    }

    CHECK(log.empty());

    shutdown();
    CHECK(log == std::vector<std::string>{ "beta:stop", "alpha:stop" });
}

TEST_CASE("ScriptingLayer never starts a runtime that has no sources")
{
    TempDir dir;
    dir.write("only.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_two_runtimes(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    {
        LayerStack stack;
        stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());
        CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:only.alpha" });
        log.clear();
    }

    shutdown();
    CHECK(log == std::vector<std::string>{ "alpha:stop" });
}

TEST_CASE("ScriptingLayer logs a script's error and keeps loading the remaining sources")
{
    TempDir dir;
    dir.write("a.alpha");
    dir.write("b-broken.alpha");
    dir.write("c.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_one_runtime(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());

    CHECK_FALSE(layer.is_disabled());
    CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:a.alpha", "alpha:load:b-broken.alpha", "alpha:load:c.alpha" });
}

TEST_CASE("ScriptingLayer skips an explicit file no runtime claims")
{
    TempDir dir;
    dir.write("thing.lua");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_one_runtime(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    ScriptDiscoveryOptions options = options_for(dir);
    options.script_files = { "thing.lua" };

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options, fakes.pointers());

    CHECK_FALSE(layer.is_disabled());
    CHECK(log.empty());
}

TEST_CASE("ScriptingLayer routes a --module to the only registered runtime")
{
    TempDir dir;
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_one_runtime(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    ScriptDiscoveryOptions options = options_for(dir);
    options.modules = { "my_game" };

    LayerStack stack;
    stack.push_layer<ScriptingLayer>(options, fakes.pointers());

    CHECK(log == std::vector<std::string>{ "alpha:start", "alpha:load:my_game" });
}

TEST_CASE("ScriptingLayer does not guess a runtime for a --module when several are registered")
{
    TempDir dir;
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_two_runtimes(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    ScriptDiscoveryOptions options = options_for(dir);
    options.modules = { "my_game" };

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options, fakes.pointers());

    CHECK_FALSE(layer.is_disabled());
    CHECK(log.empty());
}

TEST_CASE("ScriptingLayer is disabled by the layer boundary when a runtime fails to start")
{
    TempDir dir;
    dir.write("a.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_two_runtimes(fakes, log, /*fail_alpha_start=*/true);
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());

    CHECK(layer.is_disabled());
    CHECK(log == std::vector<std::string>{ "alpha:start" });
    CHECK_NOTHROW(stack.update());
}

TEST_CASE("Destroying the ScriptingLayer leaves the runtimes running until shutdown")
{
    TempDir dir;
    dir.write("a.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_one_runtime(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    {
        LayerStack stack;
        stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());
    }
    CHECK(fakes.pointers().front()->running());

    shutdown();
    CHECK_FALSE(fakes.pointers().front()->running());
    CHECK(std::count(log.begin(), log.end(), "alpha:stop") == 1);
}

TEST_CASE("ScriptingLayer with no runtimes attaches harmlessly")
{
    TempDir dir;
    dir.write("a.alpha");

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), std::vector<IScriptRuntime*>{});

    CHECK_FALSE(layer.is_disabled());
}

TEST_CASE("ScriptingLayer reload unloads each running runtime and reloads its sources in discovery order")
{
    TempDir dir;
    dir.write("a.alpha");
    dir.write("b.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_two_runtimes(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());
    log.clear();

    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(log == std::vector<std::string>{ "alpha:unload", "alpha:reload:a.alpha", "alpha:reload:b.alpha" });
    CHECK_FALSE(event.handled);
}

TEST_CASE("ScriptingLayer reload discovers new sources and starts a runtime that had none")
{
    TempDir dir;
    dir.write("a.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_two_runtimes(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());
    log.clear();

    dir.write("c.beta");
    dir.write("b.alpha");
    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(log == std::vector<std::string>{ "alpha:unload", "alpha:reload:a.alpha", "alpha:reload:b.alpha",
                                           "beta:start", "beta:load:c.beta" });
}

TEST_CASE("ScriptingLayer reload still unloads a runtime whose sources were all deleted")
{
    TempDir dir;
    std::filesystem::path file = dir.write("a.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_one_runtime(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());
    log.clear();

    std::filesystem::remove(file);
    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK(log == std::vector<std::string>{ "alpha:unload" });
}

TEST_CASE("ScriptingLayer reload logs a script's error and keeps reloading the remaining sources")
{
    TempDir dir;
    dir.write("a.alpha");
    dir.write("b-broken.alpha");
    dir.write("c.alpha");
    std::vector<std::string> log;
    FakeRuntimeSet fakes;
    add_one_runtime(fakes, log);
    ScopedScriptingShutdown shutdown_scope;

    LayerStack stack;
    ScriptingLayer& layer = stack.push_layer<ScriptingLayer>(options_for(dir), fakes.pointers());
    log.clear();

    ReloadScriptsEvent event;
    stack.dispatch_event(event);

    CHECK_FALSE(layer.is_disabled());
    CHECK(log == std::vector<std::string>{ "alpha:unload", "alpha:reload:a.alpha", "alpha:reload:b-broken.alpha", "alpha:reload:c.alpha" });
}
