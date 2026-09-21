#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Scripting/ScriptSource.h"

namespace oryx
{

class IScriptRuntime
{
public:
    virtual ~IScriptRuntime() = default;

    virtual std::string language() const = 0;
    virtual std::vector<std::string> file_patterns() const = 0;

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual void load(const ScriptSource& source) = 0;
    virtual void reload(const ScriptSource& source) = 0;

    // Drops every game and strategy this runtime's scripts registered, so a reload can re-register them.
    virtual void unload() = 0;
};

} // namespace oryx
