#include "oxpch.h"
#include "Oryx/Core/Assert.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

AssertionHandler g_handler = &trap_on_assertion;

std::string assertion_text(std::string_view detail)
{
    return detail.empty() ? "Assertion failed" : "Assertion failed: " + std::string(detail);
}

} // namespace

void set_assertion_handler(AssertionHandler handler)
{
    g_handler = handler;
}

AssertionHandler assertion_handler()
{
    return g_handler;
}

void trap_on_assertion(std::string_view message)
{
    OX_CORE_ERROR("{}", message);
    OX_DEBUGBREAK();
}

void throw_on_assertion(std::string_view message)
{
    throw AssertionError(std::string(message));
}

void assertion_failed(std::string_view message)
{
    g_handler(assertion_text(message));
}

void assertion_failed_expression(const char* expression, const char* file, int32_t line)
{
    g_handler("Assertion '" + std::string(expression) + "' failed at " + std::filesystem::path(file).filename().string() + ":" + std::to_string(line));
}

void check_failed(std::string_view message)
{
    throw AssertionError(assertion_text(message));
}

} // namespace oryx
