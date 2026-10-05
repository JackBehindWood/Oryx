#include "doctest.h"

#include "Oryx.h"

namespace
{

oryx::CommandLine make_cli()
{
    oryx::CommandLine cli("Test", "summary");
    cli.flag("headless", "no window", { "console" })
        .option("game", "NAME", "game")
        .option("script", "FILE", "script");
    return cli;
}

} // namespace

TEST_CASE("CommandLine parses flags, aliases and both value forms")
{
    oryx::ParsedArgs parsed = make_cli().parse(std::vector<std::string>{ "--console", "--game=nim" });
    CHECK(parsed.has("headless"));
    CHECK(parsed.value("game") == "nim");

    parsed = make_cli().parse(std::vector<std::string>{ "--game", "tictactoe" });
    CHECK(!parsed.has("headless"));
    CHECK(parsed.value("game") == "tictactoe");
    CHECK(parsed.value_or("missing", "fallback") == "fallback");
}

TEST_CASE("CommandLine rejects bad input ")
{
    oryx::CommandLine cli = make_cli();
    CHECK_THROWS_AS(cli.parse(std::vector<std::string>{ "--nope" }), oryx::Error);
    CHECK_THROWS_AS(cli.parse(std::vector<std::string>{ "--game" }), oryx::Error);
    CHECK_THROWS_AS(cli.parse(std::vector<std::string>{ "--headless=1" }), oryx::Error);
    CHECK_THROWS_AS(cli.parse(std::vector<std::string>{ "stray" }), oryx::Error);
}

TEST_CASE("CommandLine usage lists every option")
{
    std::string usage = make_cli().usage();
    CHECK(usage.find("--headless, --console") != std::string::npos);
    CHECK(usage.find("--game=<NAME>") != std::string::npos);
}

TEST_CASE("ParsedArgs keeps every value of a repeated option, value() the last")
{
    oryx::ParsedArgs parsed = make_cli().parse(std::vector<std::string>{ "--script", "a.py", "--script=b.py" });
    CHECK(parsed.values("script") == std::vector<std::string>{ "a.py", "b.py" });
    CHECK(parsed.value("script") == "b.py");
    CHECK(parsed.values("game").empty());
}

namespace
{

class TestContributor : public oryx::ICommandLineContributor
{
public:
    void declare(oryx::CommandLine& command_line) const override { command_line.flag("contributed-by-test", "added by a test"); }
};

} // namespace

TEST_CASE("CommandLine::global declares the options of every registered contributor")
{
    oryx::CommandLineContributors::register_factory("test", [](const oryx::Params&) -> oryx::UniquePtr<oryx::ICommandLineContributor> { return oryx::create_unique<TestContributor>(); });
    oryx::CommandLine global = oryx::CommandLine::global("test");
    oryx::CommandLineContributors::unregister_factory("test");

    CHECK_NOTHROW(global.parse(std::vector<std::string>{ "--contributed-by-test", "--settings=x.yaml", "--game=nim", "--script", "a.py" }));
    CHECK_THROWS_AS(global.parse(std::vector<std::string>{ "--nope" }), oryx::Error);
}
