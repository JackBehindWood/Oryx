#include "doctest.h"

#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("script_check does nothing when the condition holds")
{
    ClientLogCapture capture;

    CHECK_NOTHROW(script_check(true, "fine"));
    CHECK(capture.lines().empty());
}

TEST_CASE("script_check logs then throws an AssertionError carrying the message")
{
    ClientLogCapture capture;

    CHECK_THROWS_WITH_AS(script_check(false, "nope"), "Assertion failed: nope", AssertionError);
    CHECK(capture.lines() == std::vector<std::string>{ "error|Assertion failed: nope" });
}

TEST_CASE("script_check without a message still says the assertion failed")
{
    ClientLogCapture capture;

    CHECK_THROWS_WITH_AS(script_check(false, ""), "Assertion failed", AssertionError);
}

TEST_CASE("AssertionError is an Error with the 'assertion' category")
{
    AssertionError error("x");
    CHECK(std::string(error.category()) == "assertion");
    CHECK(dynamic_cast<const Error*>(&error) != nullptr);
}
