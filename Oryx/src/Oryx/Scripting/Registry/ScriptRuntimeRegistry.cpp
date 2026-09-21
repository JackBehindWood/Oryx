#include "oxpch.h"
#include "Oryx/Scripting/Registry/ScriptRuntimeRegistry.h"

#include "Oryx/Core/Application.h"

namespace oryx
{

std::vector<IScriptRuntime*> ScriptRuntimeRegistry::runtimes()
{
    static std::vector<UniquePtr<IScriptRuntime>> owned = []
    {
        std::vector<std::string> registered = names();
        std::sort(registered.begin(), registered.end());

        std::vector<UniquePtr<IScriptRuntime>> created;
        created.reserve(registered.size());
        for (const std::string& name : registered)
        {
            created.push_back(create(name));
        }
        return created;
    }();

    std::vector<IScriptRuntime*> result;
    result.reserve(owned.size());
    for (const UniquePtr<IScriptRuntime>& runtime : owned)
    {
        result.push_back(runtime.get());
    }
    return result;
}

void ScriptRuntimeRegistry::start(IScriptRuntime& runtime)
{
    if (runtime.running())
    {
        return;
    }

    runtime.start();
    register_shutdown_hook([&runtime]
    {
        if (runtime.running())
        {
            runtime.stop();
        }
    });
}

} // namespace oryx
