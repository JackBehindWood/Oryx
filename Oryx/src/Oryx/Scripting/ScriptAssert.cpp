#include "oxpch.h"
#include "Oryx/Scripting/ScriptAssert.h"

namespace oryx
{

void script_check(bool condition, std::string_view message)
{
    if (condition) [[likely]]
    {
        return;
    }

    std::string text = message.empty() ? "Assertion failed" : "Assertion failed: " + std::string(message);
    OX_ERROR("{}", text);
    throw AssertionError(text);
}

} // namespace oryx
