#include "doctest.h"

#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("Log::message routes each level to the client logger")
{
    ClientLogCapture capture;

    Log::message(spdlog::level::trace, "t");
    Log::message(spdlog::level::info, "i");
    Log::message(spdlog::level::warn, "w");
    Log::message(spdlog::level::err, "e");
    Log::message(spdlog::level::critical, "c");

    CHECK(capture.lines() == std::vector<std::string>{ "trace|t", "info|i", "warning|w", "error|e", "critical|c" });
}

TEST_CASE("Log::message never treats the text as a format string")
{
    ClientLogCapture capture;

    Log::message(spdlog::level::info, "{} {0} {name}");

    CHECK(capture.lines() == std::vector<std::string>{ "info|{} {0} {name}" });
}
