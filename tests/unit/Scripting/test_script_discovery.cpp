#include "doctest.h"

#include "ScriptingTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

std::vector<ScriptFilePattern> python_patterns()
{
    return { { "python", "*.oryx.py" } };
}

ScriptDiscoveryOptions options_for(const TempDir& dir)
{
    ScriptDiscoveryOptions options;
    options.root = dir.path();
    return options;
}

std::string relative_to(const TempDir& dir, const std::string& target)
{
    return std::filesystem::path(target).lexically_relative(dir.path()).string();
}

std::vector<std::string> relative_targets(const TempDir& dir, const std::vector<ScriptSource>& sources)
{
    std::vector<std::string> result;
    for (const ScriptSource& source : sources)
    {
        result.push_back(relative_to(dir, source.target));
    }
    return result;
}

ApplicationCommandLineArgs make_args(std::vector<std::string>& storage, std::vector<char*>& pointers)
{
    for (std::string& text : storage)
    {
        pointers.push_back(text.data());
    }
    return ApplicationCommandLineArgs{ static_cast<int32_t>(pointers.size()), pointers.data() };
}

} // namespace

TEST_CASE("matches_pattern supports leading, middle and trailing wildcards")
{
    CHECK(matches_pattern("nim.oryx.py", "*.oryx.py"));
    CHECK_FALSE(matches_pattern("nim.py", "*.oryx.py"));
    CHECK_FALSE(matches_pattern("oryx.py", "*.oryx.py"));
    CHECK(matches_pattern("nim_game.py", "nim_*.py"));
    CHECK(matches_pattern("nim.oryx.py", "nim.*"));
    CHECK(matches_pattern("exact.txt", "exact.txt"));
    CHECK_FALSE(matches_pattern("exact.txt", "exact.tx"));
    CHECK(matches_pattern("anything", "*"));
    CHECK(matches_pattern("a.b.oryx.py", "*.oryx.py"));
}

TEST_CASE("split_search_path splits on the OS separator and drops empty entries")
{
    CHECK(split_search_path("a:b::c") == std::vector<std::string>{ "a", "b", "c" });
    CHECK(split_search_path("").empty());
    CHECK(split_search_path(":").empty());
    CHECK(split_search_path("only") == std::vector<std::string>{ "only" });
}

TEST_CASE("script_options parses --script and --module in both spellings and ignores other flags")
{
    std::vector<std::string> storage = { "oasis", "--script", "a.oryx.py", "--script=b.oryx.py", "--module", "my_game",
                                         "--module=other", "--simulate=random,random,1", "--script" };
    std::vector<char*> pointers;
    ApplicationCommandLineArgs args = make_args(storage, pointers);

    ScriptDiscoveryOptions options = script_options(args);

    CHECK(options.script_files == std::vector<std::string>{ "a.oryx.py", "b.oryx.py" });
    CHECK(options.modules == std::vector<std::string>{ "my_game", "other" });
    CHECK_FALSE(options.root.empty());
}

TEST_CASE("script_options reads ORYX_SCRIPT_PATH")
{
    const char* previous = std::getenv("ORYX_SCRIPT_PATH");
    std::string saved = previous == nullptr ? "" : previous;

    setenv("ORYX_SCRIPT_PATH", "scripts:extra.oryx.py", 1);
    std::vector<std::string> storage = { "oasis" };
    std::vector<char*> pointers;
    ScriptDiscoveryOptions options = script_options(make_args(storage, pointers));
    CHECK(options.search_paths == std::vector<std::string>{ "scripts", "extra.oryx.py" });

    unsetenv("ORYX_SCRIPT_PATH");
    ScriptDiscoveryOptions without = script_options(make_args(storage, pointers));
    CHECK(without.search_paths.empty());

    if (previous != nullptr)
    {
        setenv("ORYX_SCRIPT_PATH", saved.c_str(), 1);
    }
}

TEST_CASE("discover_scripts scans the root recursively for the runtime patterns, sorted and tagged with the language")
{
    TempDir dir;
    dir.write("zeta.oryx.py");
    dir.write("scripts/nim.oryx.py");
    dir.write("scripts/deeper/alpha.oryx.py");
    dir.write("ignored.py");
    dir.write("notes.txt");

    std::vector<ScriptSource> sources = discover_scripts(options_for(dir), python_patterns());

    CHECK(relative_targets(dir, sources) == std::vector<std::string>{ "scripts/deeper/alpha.oryx.py", "scripts/nim.oryx.py", "zeta.oryx.py" });
    for (const ScriptSource& source : sources)
    {
        CHECK(source.kind == ScriptSourceKind::File);
        CHECK(source.language == "python");
    }
}

TEST_CASE("discover_scripts skips .git, virtual environments, build and bin directories")
{
    TempDir dir;
    dir.write("keep.oryx.py");
    dir.write(".git/hooks/a.oryx.py");
    dir.write(".venv/lib/b.oryx.py");
    dir.write("venv/lib/c.oryx.py");
    dir.write("custom-env/pyvenv.cfg");
    dir.write("custom-env/lib/d.oryx.py");
    dir.write("build/e.oryx.py");
    dir.write("bin/f.oryx.py");
    dir.write("bin-int/g.oryx.py");
    dir.write("src/robin/h.oryx.py");

    std::vector<ScriptSource> sources = discover_scripts(options_for(dir), python_patterns());

    CHECK(relative_targets(dir, sources) == std::vector<std::string>{ "keep.oryx.py", "src/robin/h.oryx.py" });
}

TEST_CASE("discover_scripts finds nothing to scan without patterns")
{
    TempDir dir;
    dir.write("nim.oryx.py");

    CHECK(discover_scripts(options_for(dir), {}).empty());
}

TEST_CASE("discover_scripts routes an explicit --script file by extension, even when it does not match the scan pattern")
{
    TempDir dir;
    dir.write("custom.py");
    dir.write("thing.lua");

    ScriptDiscoveryOptions options = options_for(dir);
    options.script_files = { "custom.py", "thing.lua" };

    std::vector<ScriptSource> sources = discover_scripts(options, python_patterns());

    REQUIRE(sources.size() == 2);
    CHECK(sources[0].language == "python");
    CHECK(sources[1].language.empty());
}

TEST_CASE("discover_scripts unions CLI, environment and scan sources in that order and drops duplicates")
{
    TempDir dir;
    dir.write("cli.oryx.py");
    dir.write("scan.oryx.py");
    dir.write("extra/env.oryx.py");
    dir.write("extra/deeper/env2.oryx.py");
    dir.write("single.oryx.py");

    ScriptDiscoveryOptions options = options_for(dir);
    options.script_files = { "cli.oryx.py", "cli.oryx.py" };
    options.modules = { "my_game", "my_game" };
    options.search_paths = { "extra", "single.oryx.py", "single.oryx.py" };

    std::vector<ScriptSource> sources = discover_scripts(options, python_patterns());

    REQUIRE(sources.size() == 6);
    CHECK(relative_to(dir, sources[0].target) == "cli.oryx.py");
    CHECK(sources[1].kind == ScriptSourceKind::Module);
    CHECK(sources[1].target == "my_game");
    CHECK(sources[1].language.empty());
    CHECK(relative_to(dir, sources[2].target) == "extra/deeper/env2.oryx.py");
    CHECK(relative_to(dir, sources[3].target) == "extra/env.oryx.py");
    CHECK(relative_to(dir, sources[4].target) == "single.oryx.py");
    CHECK(relative_to(dir, sources[5].target) == "scan.oryx.py");
}
