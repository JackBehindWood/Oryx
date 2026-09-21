#include "oxpch.h"
#include "Oryx/Scripting/Support/ScriptLog.h"

namespace oryx
{

void script_log(ScriptLogLevel level, std::string_view message)
{
    switch (level)
    {
    case ScriptLogLevel::Trace:
        OX_TRACE("{}", message);
        break;
    case ScriptLogLevel::Info:
        OX_INFO("{}", message);
        break;
    case ScriptLogLevel::Warn:
        OX_WARN("{}", message);
        break;
    case ScriptLogLevel::Error:
        OX_ERROR("{}", message);
        break;
    case ScriptLogLevel::Critical:
        OX_CRITICAL("{}", message);
        break;
    }
}

} // namespace oryx
