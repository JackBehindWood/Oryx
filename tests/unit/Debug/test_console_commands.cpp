#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

std::string g_seen;

OX_CONSOLE_COMMAND(test_echo, "test.echo <word> [count]", "Echoes a word")
{
    g_seen = std::string(call.arg(0));
    call.print("%s x%lld", g_seen.c_str(), static_cast<long long>(call.count() > 1 ? call.integer(1) : 1));
}

void plain_function(ConsoleCall& call)
{
    call.print("plain %s", call.count() > 0 ? std::string(call.arg(0)).c_str() : "none");
}

OX_REGISTER_CONSOLE_COMMAND(plain_function, "test.plain [what]", "A function registered by name")
OX_CONSOLE_COMMAND_ALIAS("test.alias", "test.plain")

OX_CONSOLE_COMMAND(test_choice, "test.pick <left|right|centre> <fast|slow>", "Takes choices")
{
}

class ExtraSource final : public IConsoleSource
{
public:
    bool execute(ConsoleCall& call) override
    {
        if (call.name() != "extra.go")
        {
            return false;
        }
        call.print("went");
        return true;
    }

    void names(std::vector<std::string>& out) const override { out.push_back("extra.go"); }
};

} // namespace

#ifdef OX_ENABLE_CONSOLE_COMMANDS

TEST_CASE("console: a command registered with the macro runs with its arguments")
{
    const ConsoleResult result = console::run("test.echo hello 3");
    CHECK(result.ok);
    CHECK(result.output == "hello x3\n");
    CHECK(g_seen == "hello");
}

TEST_CASE("console: a function and an alias register the same way")
{
    CHECK(console::run("test.plain a").output == "plain a\n");
    CHECK(console::run("test.alias").output == "plain none\n");
}

TEST_CASE("console: a failing command prints its error instead of throwing")
{
    ConsoleResult missing = console::run("test.echo");
    CHECK_FALSE(missing.ok);
    CHECK(missing.output.find("missing argument 1") != std::string::npos);
    ConsoleResult bad_number = console::run("test.echo w nope");
    CHECK_FALSE(bad_number.ok);
    CHECK(bad_number.output.find("not a whole number") != std::string::npos);
    CHECK_FALSE(console::run("no.such.command").ok);
    CHECK(console::run("   ").output.empty());
}

TEST_CASE("console: an outside source runs after the table and shows in completion")
{
    ExtraSource source;
    CHECK(console::run("extra.go", &source).output == "went\n");
    CHECK_FALSE(console::run("extra.go").ok);
    std::string line = "extra.g";
    std::vector<std::string> options;
    CHECK(console::complete(line, options, &source));
    CHECK(line == "extra.go ");
}

TEST_CASE("console: completion extends names and offers a usage's choices")
{
    std::vector<std::string> options;
    std::string line = "test.e";
    CHECK(console::complete(line, options));
    CHECK(line == "test.echo ");

    line = "test.p";
    CHECK_FALSE(console::complete(line, options));
    CHECK(line == "test.p");
    CHECK(options.size() >= 2);

    line = "test.pick ce";
    CHECK(console::complete(line, options));
    CHECK(line == "test.pick centre ");

    line = "test.pick centre ";
    CHECK_FALSE(console::complete(line, options));
    CHECK(options == std::vector<std::string>{ "fast", "slow" });
    CHECK(line == "test.pick centre ");

    line = "zzz";
    CHECK_FALSE(console::complete(line, options));
}

TEST_CASE("console: built-ins help, clear and log.level work")
{
    CHECK(console::run("help").output.find("log.level") != std::string::npos);
    CHECK(console::run("help clear").output.find("clear") != std::string::npos);
    CHECK(console::run("clear").clear);

    const spdlog::level::level_enum before = Log::get_client_logger()->level();
    CHECK(console::run("log.level app error").ok);
    CHECK(Log::get_client_logger()->level() == spdlog::level::err);
    CHECK_FALSE(console::run("log.level app loud").ok);
    CHECK_FALSE(console::run("log.level nobody info").ok);
    Log::get_client_logger()->set_level(before);
}

#else

TEST_CASE("console: a build without commands answers that they are not built")
{
    const ConsoleResult result = console::run("help");
    CHECK_FALSE(result.ok);
    CHECK(result.output.find("not built") != std::string::npos);
}

#endif

TEST_CASE("console: history keeps the last lines, skips blanks and repeats, and walks both ways")
{
    ConsoleHistory history;
    history.push("one");
    history.push("");
    history.push("two");
    history.push("two");
    CHECK(history.size() == 2);
    CHECK(history.previous() == "two");
    CHECK(history.previous() == "one");
    CHECK(history.previous() == "one");
    CHECK(history.next() == "two");
    CHECK(history.next().empty());

    for (uint32_t i = 0; i < k_console_history + 10; ++i)
    {
        history.push("line" + std::to_string(i));
    }
    CHECK(history.size() == k_console_history);
    CHECK(history.previous() == "line" + std::to_string(k_console_history + 9));
}
