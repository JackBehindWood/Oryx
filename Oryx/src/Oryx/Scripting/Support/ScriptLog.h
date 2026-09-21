#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

enum class ScriptLogLevel
{
    Trace,
    Info,
    Warn,
    Error,
    Critical
};

void script_log(ScriptLogLevel level, std::string_view message);

} // namespace oryx
