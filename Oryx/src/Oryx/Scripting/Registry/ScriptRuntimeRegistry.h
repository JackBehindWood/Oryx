#pragma once

#include "Oryx/Core/Registry.h"
#include "Oryx/Scripting/Interfaces/IScriptRuntime.h"

namespace oryx
{

class ScriptRuntimeRegistry : public Registry<IScriptRuntime>
{
public:
    // One runtime per registered name for the whole process, in sorted-name order; created on first use.
    static std::vector<IScriptRuntime*> runtimes();

    // Starts the runtime unless it already runs and has oryx::shutdown() stop it; the runtime must outlive shutdown().
    static void start(IScriptRuntime& runtime);
};

} // namespace oryx

#define OX_REGISTER_SCRIPT_RUNTIME(Type, name) OX_REGISTER_FACTORY(::oryx::IScriptRuntime, Type, name)
