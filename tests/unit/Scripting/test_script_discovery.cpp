#include "doctest.h"

#include "ScriptingTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

std::vector<ScriptFileExtension> python_extensions()
{
    return { { "python", ".py" } };
}

ScriptDiscoveryOptions options_for(const TempDir& dir)
{
    ScriptDiscoveryOptions options;
    options.root = dir.path();
    return options;
}

std::string relative_to(const TempDir& dir, const std::string& target)
{
    return std::filesystem::path(target).lexically_relative(dir.path()).generic_string();
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

class Arguments
{
public:
    explicit Arguments(std::vector<std::string> values)
        : m_values(std::move(values))
    {
        for (std::string& text : m_values)
        {
            m_pointers.push_back(text.data());
        }
    }

    [[nodiscard]] ApplicationCommandLineArgs get() { return ApplicationCommandLineArgs{ static_cast<int32_t>(m_pointers.size()), m_pointers.data() }; }

private:
    std::vector<std::string> m_values;
    std::vector<char*> m_pointers;
};

bool logged(const CoreLogCapture& capture, const std::string& text)
{
    std::vector<std::string> lines = capture.lines();
    return std::any_of(lines.begin(), lines.end(), [&text](const std::string& line) { return line.find(text) != std::string::npos; });
}

} // namespace

TEST_CASE("script_options reads --script, --module and --script-root, as separate or joined values")
{
    Arguments args({ "app", "--script", "a.py", "--script=b.py", "--module", "mod.one", "--module=mod.two", "--script-root", "scripts", "--script-root=more", "--unrelated", "x" });
    ScriptDiscoveryOptions options = script_options(args.get());

    CHECK(options.script_files == std::vector<std::string>{ "a.py", "b.py" });
    CHECK(options.modules == std::vector<std::string>{ "mod.one", "mod.two" });
    CHECK(options.roots == std::vector<std::string>{ "scripts", "more" });
    CHECK_FALSE(options.root.empty());
}

TEST_CASE("script_options warns about a flag that has no value and ignores it")
{
    CoreLogCapture capture;
    Arguments args({ "app", "--script" });
    ScriptDiscoveryOptions options = script_options(args.get());

    CHECK(options.script_files.empty());
    CHECK(logged(capture, "--script needs a value"));
}

TEST_CASE("discover_scripts finds every script under a root in path order, with the root it is imported from")
{
    TempDir dir;
    dir.write("scripts/zeta.py");
    dir.write("scripts/nim.py");
    dir.write("scripts/deeper/alpha.py");
    dir.write("scripts/notes.txt");

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "scripts" };
    std::vector<ScriptSource> sources = discover_scripts(options, python_extensions());

    CHECK(relative_targets(dir, sources) == std::vector<std::string>{ "scripts/deeper/alpha.py", "scripts/nim.py", "scripts/zeta.py" });
    for (const ScriptSource& source : sources)
    {
        CHECK(source.kind == ScriptSourceKind::File);
        CHECK(source.language == "python");
        CHECK(source.root == (dir.path() / "scripts").string());
    }
}

TEST_CASE("a file or directory starting with an underscore or a dot is a helper and is never loaded on its own")
{
    TempDir dir;
    dir.write("scripts/game.py");
    dir.write("scripts/_common.py");
    dir.write("scripts/_helpers/inner.py");
    dir.write("scripts/__pycache__/game.cpython-311.py");
    dir.write("scripts/.hidden/secret.py");
    dir.write("scripts/pkg/_private.py");
    dir.write("scripts/pkg/public.py");

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "scripts" };

    CHECK(relative_targets(dir, discover_scripts(options, python_extensions())) == std::vector<std::string>{ "scripts/game.py", "scripts/pkg/public.py" });
}

TEST_CASE("only files with an extension some runtime claims are scripts, and each gets that runtime's language")
{
    TempDir dir;
    dir.write("root/a.py");
    dir.write("root/b.lua");
    dir.write("root/c.txt");

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "root" };
    std::vector<ScriptSource> sources = discover_scripts(options, { { "python", ".py" }, { "lua", ".lua" } });

    REQUIRE(sources.size() == 2);
    CHECK(sources[0].language == "python");
    CHECK(sources[1].language == "lua");
}

TEST_CASE("a root that is missing or is a file is warned about and skipped, and no root means nothing is scanned")
{
    TempDir dir;
    dir.write("scripts/nim.py");
    dir.write("just-a-file.py");
    CoreLogCapture capture;

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "nowhere", "just-a-file.py", "scripts" };

    CHECK(relative_targets(dir, discover_scripts(options, python_extensions())) == std::vector<std::string>{ "scripts/nim.py" });
    CHECK(logged(capture, "nowhere"));
    CHECK(logged(capture, "just-a-file.py"));

    CHECK(discover_scripts(options_for(dir), python_extensions()).empty());
}

TEST_CASE("relative roots resolve against the working directory and absolute roots are used as they are")
{
    TempDir dir;
    TempDir other;
    dir.write("here/a.py");
    other.write("b.py");

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "here", other.path().string() };
    std::vector<ScriptSource> sources = discover_scripts(options, python_extensions());

    REQUIRE(sources.size() == 2);
    CHECK(sources[0].target == (dir.path() / "here/a.py").string());
    CHECK(sources[1].target == (other.path() / "b.py").string());
    CHECK(sources[1].root == other.path().string());
}

TEST_CASE("a --script inside a root is imported from that root, and one outside uses its own directory")
{
    TempDir dir;
    dir.write("scripts/deep/inside.py");
    dir.write("loose/outside.py");

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "scripts" };
    options.script_files = { "scripts/deep/inside.py", "loose/outside.py", "missing.txt" };
    std::vector<ScriptSource> sources = discover_scripts(options, python_extensions());

    REQUIRE(sources.size() == 3);
    CHECK(sources[0].root == (dir.path() / "scripts").string());
    CHECK(sources[1].root == (dir.path() / "loose").string());
    CHECK(sources[1].language == "python");
    CHECK(sources[2].language.empty());
}

TEST_CASE("sources come out as --script files, then --module names, then the roots' scripts, without duplicates")
{
    TempDir dir;
    dir.write("scripts/a.py");
    dir.write("scripts/b.py");

    ScriptDiscoveryOptions options = options_for(dir);
    options.roots = { "scripts", "scripts" };
    options.script_files = { "scripts/b.py", "scripts/b.py" };
    options.modules = { "pkg.game", "pkg.game" };
    std::vector<ScriptSource> sources = discover_scripts(options, python_extensions());

    REQUIRE(sources.size() == 3);
    CHECK(relative_to(dir, sources[0].target) == "scripts/b.py");
    CHECK(sources[1].kind == ScriptSourceKind::Module);
    CHECK(sources[1].target == "pkg.game");
    CHECK(sources[1].root.empty());
    CHECK(relative_to(dir, sources[2].target) == "scripts/a.py");
}

TEST_CASE("the scripting settings section reads its roots relative to the settings file")
{
    TempDir dir;
    dir.write("oryx.yaml", "scripting:\n  roots:\n    - scripts\n    - ../shared\n");
    std::string flag = "--settings=" + (dir.path() / "oryx.yaml").string();
    Arguments args({ "app", flag });
    load_settings(args.get());

    const ScriptSettings& settings = settings_of<ScriptSettings>();
    REQUIRE(settings.roots.size() == 2);
    CHECK(settings.roots[0] == (dir.path() / "scripts").lexically_normal());
    CHECK(settings.roots[1] == (dir.path() / "../shared").lexically_normal());

    reset_settings();
    CHECK(settings_of<ScriptSettings>().roots.empty());
}
