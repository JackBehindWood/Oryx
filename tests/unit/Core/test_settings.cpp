#include "doctest.h"

#include "unit/Scripting/ScriptingTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct FakeSettings
{
    int64_t count = 7;
    std::string label = "default";
    bool flag = false;
    std::vector<std::string> names;
    std::vector<std::filesystem::path> dirs;
    std::filesystem::path file = "none";
    bool has_extra = false;
};

void read_settings(FakeSettings& settings, const SettingsNode& node)
{
    settings.count = node.integer("count", settings.count);
    settings.label = node.string("label", settings.label);
    settings.flag = node.boolean("flag", settings.flag);
    settings.names = node.strings("names");
    settings.dirs = node.paths("dirs");
    settings.file = node.path("file", settings.file);
    settings.has_extra = node.has("extra");
}

struct UnregisteredSettings
{
};

class Arguments
{
public:
    explicit Arguments(std::vector<std::string> values)
        : m_values(std::move(values))
    {
        for (std::string& value : m_values)
        {
            m_pointers.push_back(value.data());
        }
    }

    [[nodiscard]] ApplicationCommandLineArgs get() { return { static_cast<int32_t>(m_pointers.size()), m_pointers.data() }; }

private:
    std::vector<std::string> m_values;
    std::vector<char*> m_pointers;
};

void load_from(const std::filesystem::path& file)
{
    Arguments args({ "app", "--settings=" + file.string() });
    load_settings(args.get());
}

std::string load_error(const std::filesystem::path& file)
{
    try
    {
        load_from(file);
    }
    catch (const SettingsError& error)
    {
        return error.what();
    }
    return "";
}

bool logged(const CoreLogCapture& capture, const std::string& text)
{
    std::vector<std::string> lines = capture.lines();
    return std::any_of(lines.begin(), lines.end(), [&text](const std::string& line) { return line.find(text) != std::string::npos; });
}

} // namespace

OX_REGISTER_SETTINGS(FakeSettings, "fake")

TEST_CASE("a registered section has its defaults until a file is loaded, and a file without the section keeps them")
{
    TempDir dir;
    load_from(dir.write("empty.yaml", "other: {}\n"));

    const FakeSettings& settings = settings_of<FakeSettings>();
    CHECK(settings.count == 7);
    CHECK(settings.label == "default");
    CHECK_FALSE(settings.flag);
    CHECK(settings.names.empty());
    CHECK(settings.file == std::filesystem::path("none"));
}

TEST_CASE("settings_of throws a SettingsError for a type that has no registered section")
{
    CHECK_THROWS_AS(settings_of<UnregisteredSettings>(), SettingsError);
}

TEST_CASE("every typed getter reads its value from the section")
{
    TempDir dir;
    load_from(dir.write("all.yaml",
        "fake:\n"
        "  count: 42\n"
        "  label: hello\n"
        "  flag: true\n"
        "  names: [a, b]\n"
        "  extra: 1\n"));

    const FakeSettings& settings = settings_of<FakeSettings>();
    CHECK(settings.count == 42);
    CHECK(settings.label == "hello");
    CHECK(settings.flag);
    CHECK(settings.names == std::vector<std::string>{ "a", "b" });
    CHECK(settings.has_extra);
}

TEST_CASE("relative paths resolve against the settings file's directory and absolute ones stay as written")
{
    TempDir dir;
    std::filesystem::path absolute = std::filesystem::temp_directory_path() / "elsewhere";
    load_from(dir.write("paths.yaml",
        "fake:\n"
        "  dirs: [scripts, ../up, " + absolute.generic_string() + "]\n"
        "  file: data/one.txt\n"));

    const FakeSettings& settings = settings_of<FakeSettings>();
    REQUIRE(settings.dirs.size() == 3);
    CHECK(settings.dirs[0] == (dir.path() / "scripts").lexically_normal());
    CHECK(settings.dirs[1] == (dir.path() / "../up").lexically_normal());
    CHECK(settings.dirs[2] == absolute);
    CHECK(settings.file == (dir.path() / "data/one.txt").lexically_normal());
}

TEST_CASE("an unknown key inside a section and an unknown section are warned about, naming file and line")
{
    TempDir dir;
    CoreLogCapture capture;
    load_from(dir.write("typos.yaml",
        "fake:\n"
        "  count: 1\n"
        "  cuont: 2\n"
        "stray:\n"
        "  a: 1\n"));

    CHECK(logged(capture, "unknown key 'cuont' in section 'fake'"));
    CHECK(logged(capture, "typos.yaml:3"));
    CHECK(logged(capture, "unknown section 'stray'"));
    CHECK_FALSE(logged(capture, "unknown key 'count'"));
    CHECK(settings_of<FakeSettings>().count == 1);
}

TEST_CASE("a value of the wrong type is a SettingsError naming the file, line and key")
{
    TempDir dir;
    std::string message = load_error(dir.write("wrong.yaml",
        "fake:\n"
        "  count: many\n"));

    CHECK(message.find("wrong.yaml:2") != std::string::npos);
    CHECK(message.find("fake.count must be an integer") != std::string::npos);

    CHECK(load_error(dir.write("list.yaml", "fake:\n  names: nope\n")).find("fake.names must be a list of strings") != std::string::npos);
    CHECK(load_error(dir.write("bool.yaml", "fake:\n  flag: maybe\n")).find("fake.flag must be true or false") != std::string::npos);
}

TEST_CASE("malformed YAML, a non-mapping file and a non-mapping section are SettingsErrors")
{
    TempDir dir;
    CHECK(load_error(dir.write("bad.yaml", "fake:\n  count: [1\n")).find("bad.yaml:") != std::string::npos);
    CHECK(load_error(dir.write("scalar.yaml", "just text\n")).find("must be a mapping of section names") != std::string::npos);
    CHECK(load_error(dir.write("section.yaml", "fake: 3\n")).find("section 'fake' must be a mapping") != std::string::npos);
}

TEST_CASE("--settings names a file that has to exist")
{
    TempDir dir;
    CHECK(load_error(dir.path() / "missing.yaml").find("settings file not found") != std::string::npos);
}

TEST_CASE("without --settings the file is oryx.yaml in the working directory, and a missing one means defaults")
{
    TempDir dir;
    std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(dir.path());

    Arguments none({ "app" });
    load_settings(none.get());
    CHECK(settings_of<FakeSettings>().count == 7);

    dir.write("oryx.yaml", "fake:\n  count: 5\n");
    load_settings(none.get());
    CHECK(settings_of<FakeSettings>().count == 5);

    std::filesystem::current_path(previous);
}

TEST_CASE("an application's registered default file is used before oryx.yaml, and --settings still wins")
{
    TempDir dir;
    std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(dir.path());
    dir.write("oryx.yaml", "fake:\n  count: 5\n");
    std::filesystem::path app_file = dir.write("app/oryx.yaml", "fake:\n  count: 8\n");

    register_default_settings_file("app/missing.yaml");
    register_default_settings_file("app/oryx.yaml");
    Arguments none({ "app" });
    load_settings(none.get());
    CHECK(settings_of<FakeSettings>().count == 8);

    load_from(dir.write("chosen.yaml", "fake:\n  count: 9\n"));
    CHECK(settings_of<FakeSettings>().count == 9);

    reset_settings();
    load_settings(none.get());
    CHECK(settings_of<FakeSettings>().count == 5);

    std::filesystem::current_path(previous);
    reset_settings();
}

TEST_CASE("loading again resets a key the new file no longer sets")
{
    TempDir dir;
    load_from(dir.write("first.yaml", "fake:\n  label: one\n  count: 3\n"));
    CHECK(settings_of<FakeSettings>().label == "one");

    load_from(dir.write("second.yaml", "fake:\n  count: 4\n"));
    CHECK(settings_of<FakeSettings>().label == "default");
    CHECK(settings_of<FakeSettings>().count == 4);
}

TEST_CASE("reload applies edits in place, so a reference taken earlier sees them")
{
    TempDir dir;
    std::filesystem::path file = dir.write("live.yaml", "fake:\n  count: 1\n");
    load_from(file);
    const FakeSettings& before = settings_of<FakeSettings>();
    CHECK(before.count == 1);

    dir.write("live.yaml", "fake:\n  count: 2\n  label: edited\n");
    reload_settings();

    CHECK(before.count == 2);
    CHECK(before.label == "edited");
}

TEST_CASE("a failed reload keeps the values that were in effect")
{
    TempDir dir;
    std::filesystem::path file = dir.write("keep.yaml", "fake:\n  count: 11\n");
    load_from(file);

    dir.write("keep.yaml", "fake:\n  count: [broken\n");
    CHECK_THROWS_AS(reload_settings(), SettingsError);
    CHECK(settings_of<FakeSettings>().count == 11);

    dir.write("keep.yaml", "fake:\n  count: oops\n");
    CHECK_THROWS_AS(reload_settings(), SettingsError);
    CHECK(settings_of<FakeSettings>().count == 11);

    dir.write("keep.yaml", "fake:\n  count: 12\n");
    CHECK_NOTHROW(reload_settings());
    CHECK(settings_of<FakeSettings>().count == 12);
}

TEST_CASE("reload does nothing before a load, and reset returns every section to its defaults and forgets the file")
{
    reset_settings();
    CHECK_NOTHROW(reload_settings());

    TempDir dir;
    load_from(dir.write("some.yaml", "fake:\n  count: 9\n"));
    CHECK(settings_of<FakeSettings>().count == 9);

    reset_settings();
    CHECK(settings_of<FakeSettings>().count == 7);
    CHECK_NOTHROW(reload_settings());
    CHECK(settings_of<FakeSettings>().count == 7);
}

TEST_CASE("a failed first load leaves the defaults untouched")
{
    TempDir dir;
    load_from(dir.write("reset.yaml", "fake:\n  count: 1\n"));
    CHECK_THROWS_AS(load_from(dir.write("broken.yaml", "fake:\n  count: no\n")), SettingsError);
    CHECK(settings_of<FakeSettings>().count == 1);
}
