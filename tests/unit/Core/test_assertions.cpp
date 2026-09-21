#include "doctest.h"

#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

std::string g_seen;

void record_assertion(std::string_view message)
{
    g_seen = std::string(message);
}

class HandlerScope
{
public:
    explicit HandlerScope(AssertionHandler handler)
        : m_previous(assertion_handler())
    {
        set_assertion_handler(handler);
    }

    ~HandlerScope() { set_assertion_handler(m_previous); }

    HandlerScope(const HandlerScope&) = delete;
    HandlerScope& operator=(const HandlerScope&) = delete;

private:
    AssertionHandler m_previous;
};

} // namespace

TEST_CASE("check does nothing when the condition holds")
{
    ClientLogCapture client;
    CoreLogCapture core;

    CHECK_NOTHROW(check(true, "fine"));
    CHECK(client.lines().empty());
    CHECK(core.lines().empty());
}

TEST_CASE("check throws an AssertionError carrying the message and does not log")
{
    ClientLogCapture client;
    CoreLogCapture core;

    CHECK_THROWS_WITH_AS(check(false, "nope"), "Assertion failed: nope", AssertionError);
    CHECK(client.lines().empty());
    CHECK(core.lines().empty());
}

TEST_CASE("check without a message still says the assertion failed")
{
    CHECK_THROWS_WITH_AS(check(false), "Assertion failed", AssertionError);
}

TEST_CASE("check throws whatever the assertion handler is")
{
    HandlerScope scope(&record_assertion);

    CHECK_THROWS_AS(check(false, "x"), AssertionError);
    CHECK(g_seen.empty());
}

TEST_CASE("the default assertion handler traps")
{
    CHECK(assertion_handler() == &trap_on_assertion);
}

TEST_CASE("assertion_failed hands the wrapped message to the current handler")
{
    HandlerScope scope(&record_assertion);

    assertion_failed("boom");
    CHECK(g_seen == "Assertion failed: boom");
    assertion_failed("");
    CHECK(g_seen == "Assertion failed");
}

TEST_CASE("assertion_failed_expression names the expression, file and line")
{
    HandlerScope scope(&record_assertion);

    assertion_failed_expression("a == b", "some/dir/File.cpp", 42);

    CHECK(g_seen == "Assertion 'a == b' failed at File.cpp:42");
}

TEST_CASE("throw_on_assertion turns a failed assertion into an AssertionError without logging")
{
    HandlerScope scope(&throw_on_assertion);
    ClientLogCapture client;
    CoreLogCapture core;

    CHECK_THROWS_WITH_AS(assertion_failed("bad"), "Assertion failed: bad", AssertionError);
    CHECK(client.lines().empty());
    CHECK(core.lines().empty());
}

#ifdef OX_ENABLE_ASSERTS
TEST_CASE("OX_CORE_ASSERT and OX_ASSERT run the assertion handler only when the condition fails")
{
    HandlerScope scope(&throw_on_assertion);

    CHECK_NOTHROW(OX_CORE_ASSERT(1 + 1 == 2, "arithmetic"));
    CHECK_THROWS_WITH_AS(OX_CORE_ASSERT(1 + 1 == 3, "arithmetic"), "Assertion failed: arithmetic", AssertionError);
    CHECK_THROWS_AS(OX_ASSERT(false), AssertionError);
}
#endif

TEST_CASE("AssertionError is an Error with the 'assertion' category")
{
    AssertionError error("x");

    CHECK(std::string(error.category()) == "assertion");
    CHECK(dynamic_cast<const Error*>(&error) != nullptr);
}
