#include "doctest.h"

#include "Oryx.h"

#include "unit/Scripting/ScriptingTestSupport.h"

namespace oryx::test
{

class RegisteredRuntimeA : public IScriptRuntime
{
public:
    std::string language() const override { return "a"; }
    std::vector<std::string> file_extensions() const override { return { ".a" }; }
    bool running() const override { return false; }
    void start() override {}
    void stop() override {}
    void load(const ScriptSource&) override {}
    void reload(const ScriptSource&) override {}
    void unload() override {}
};

class RegisteredRuntimeB : public RegisteredRuntimeA
{
public:
    std::string language() const override { return "b"; }
    std::vector<std::string> file_extensions() const override { return { ".b" }; }
};

} // namespace oryx::test

OX_REGISTER_SCRIPT_RUNTIME(oryx::test::RegisteredRuntimeB, "test-runtime-b")
OX_REGISTER_SCRIPT_RUNTIME(oryx::test::RegisteredRuntimeA, "test-runtime-a")

TEST_CASE("OX_REGISTER_SCRIPT_RUNTIME registers into the script runtime registry")
{
    CHECK(oryx::ScriptRuntimeRegistry::has("test-runtime-a"));
    CHECK(oryx::ScriptRuntimeRegistry::has("test-runtime-b"));
    CHECK(oryx::ScriptRuntimeRegistry::create("test-runtime-a")->language() == "a");
}

TEST_CASE("ScriptRuntimeRegistry::runtimes yields one process-wide runtime per registered name, in sorted-name order")
{
    std::vector<oryx::IScriptRuntime*> runtimes = oryx::ScriptRuntimeRegistry::runtimes();

    std::vector<std::string> languages;
    for (const oryx::IScriptRuntime* runtime : runtimes)
    {
        languages.push_back(runtime->language());
    }

    REQUIRE(languages.size() == oryx::ScriptRuntimeRegistry::names().size());
    CHECK(std::find(languages.begin(), languages.end(), "a") < std::find(languages.begin(), languages.end(), "b"));
    CHECK(oryx::ScriptRuntimeRegistry::runtimes() == runtimes);
}

TEST_CASE("ScriptRuntimeRegistry::start starts a runtime once and has shutdown stop it once")
{
    std::vector<std::string> log;
    oryx::test::FakeRuntime runtime("solo", "*.solo", log);
    oryx::test::ScopedScriptingShutdown shutdown_scope;

    oryx::ScriptRuntimeRegistry::start(runtime);
    oryx::ScriptRuntimeRegistry::start(runtime);
    CHECK(runtime.running());
    CHECK(log == std::vector<std::string>{ "solo:start" });

    oryx::shutdown();
    oryx::shutdown();
    CHECK_FALSE(runtime.running());
    CHECK(log == std::vector<std::string>{ "solo:start", "solo:stop" });
}

TEST_CASE("A runtime stopped before shutdown is not stopped again")
{
    std::vector<std::string> log;
    oryx::test::FakeRuntime runtime("solo", "*.solo", log);
    oryx::test::ScopedScriptingShutdown shutdown_scope;

    oryx::ScriptRuntimeRegistry::start(runtime);
    runtime.stop();
    oryx::shutdown();

    CHECK(log == std::vector<std::string>{ "solo:start", "solo:stop" });
}
