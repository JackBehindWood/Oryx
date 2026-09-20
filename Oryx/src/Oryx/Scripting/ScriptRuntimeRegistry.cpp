#include "oxpch.h"
#include "Oryx/Scripting/ScriptRuntimeRegistry.h"

namespace oryx
{

std::vector<UniquePtr<IScriptRuntime>> ScriptRuntimeRegistry::create_all()
{
    std::vector<std::string> registered = names();
    std::sort(registered.begin(), registered.end());

    std::vector<UniquePtr<IScriptRuntime>> runtimes;
    runtimes.reserve(registered.size());
    for (const std::string& name : registered)
    {
        runtimes.push_back(create(name));
    }
    return runtimes;
}

} // namespace oryx
