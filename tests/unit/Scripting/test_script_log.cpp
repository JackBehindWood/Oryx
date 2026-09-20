#include "doctest.h"

#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("script_log routes each level to the client logger")
{
    ClientLogCapture capture;

    script_log(ScriptLogLevel::Trace, "t");
    script_log(ScriptLogLevel::Info, "i");
    script_log(ScriptLogLevel::Warn, "w");
    script_log(ScriptLogLevel::Error, "e");
    script_log(ScriptLogLevel::Critical, "c");

    CHECK(capture.lines() == std::vector<std::string>{ "trace|t", "info|i", "warning|w", "error|e", "critical|c" });
}

TEST_CASE("script_log never treats the message as a format string")
{
    ClientLogCapture capture;

    script_log(ScriptLogLevel::Info, "{} {0} {name}");

    CHECK(capture.lines() == std::vector<std::string>{ "info|{} {0} {name}" });
}
