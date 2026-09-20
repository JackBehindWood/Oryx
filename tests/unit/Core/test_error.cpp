#include "doctest.h"

#include "Oryx.h"

TEST_CASE("Error carries its category, message and detail")
{
    oryx::Error error("something failed", "line 1\nline 2");

    CHECK(std::string(error.category()) == "error");
    CHECK(std::string(error.what()) == "something failed");
    CHECK(error.detail() == "line 1\nline 2");
}

TEST_CASE("Error defaults to an empty detail")
{
    oryx::Error error("something failed");

    CHECK(error.detail().empty());
}

TEST_CASE("A derived error is caught as oryx::Error and as std::exception")
{
    CHECK_THROWS_AS(throw oryx::ParamError("entry", "key", "is unknown"), oryx::Error);
    CHECK_THROWS_AS(throw oryx::ParamError("entry", "key", "is unknown"), std::exception);
}

TEST_CASE("A derived error reports its own category through the base class")
{
    const oryx::ParamError param_error("entry", "key", "is unknown");
    const oryx::ScriptError script_error("boom");
    const oryx::Error& as_param = param_error;
    const oryx::Error& as_script = script_error;

    CHECK(std::string(as_param.category()) == "param");
    CHECK(std::string(as_script.category()) == "script");
}

TEST_CASE("Error::log reports errors with and without detail")
{
    CHECK_NOTHROW(oryx::Error("no detail").log());
    CHECK_NOTHROW(oryx::Error("with detail", "traceback text").log());
}
