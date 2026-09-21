#include "doctest.h"

#include "Oryx.h"

namespace oryx::test
{

class RegisteredRuntimeA : public IScriptRuntime
{
public:
    std::string language() const override { return "a"; }
    std::vector<std::string> file_patterns() const override { return { "*.a" }; }
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
    std::vector<std::string> file_patterns() const override { return { "*.b" }; }
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

TEST_CASE("ScriptRuntimeRegistry::create_all yields one runtime per registered name, in sorted-name order")
{
    std::vector<oryx::UniquePtr<oryx::IScriptRuntime>> runtimes = oryx::ScriptRuntimeRegistry::create_all();

    std::vector<std::string> languages;
    for (const oryx::UniquePtr<oryx::IScriptRuntime>& runtime : runtimes)
    {
        languages.push_back(runtime->language());
    }

    REQUIRE(languages.size() == oryx::ScriptRuntimeRegistry::names().size());
    CHECK(std::find(languages.begin(), languages.end(), "a") < std::find(languages.begin(), languages.end(), "b"));
}
