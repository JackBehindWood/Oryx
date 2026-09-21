#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Scripting/Support/ScriptSource.h"

namespace oryx
{

class IScriptRuntime
{
public:
    virtual ~IScriptRuntime() = default;

    virtual std::string language() const = 0;
    // File extensions this runtime runs, with the dot (".py"); used to pick scripts under a root and to route --script files.
    virtual std::vector<std::string> file_extensions() const = 0;

    [[nodiscard]] virtual bool running() const = 0;

    virtual void start() = 0;
    // Releases everything the runtime holds, including what unload() drops; called by oryx::shutdown() after every layer is gone.
    virtual void stop() = 0;

    virtual void load(const ScriptSource& source) = 0;
    virtual void reload(const ScriptSource& source) = 0;

    // Drops every game and strategy this runtime's scripts registered, so a reload can re-register them.
    virtual void unload() = 0;
};

} // namespace oryx
