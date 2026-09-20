#pragma once

#include "Oryx/Core/Registry.h"
#include "Oryx/Scripting/IScriptRuntime.h"

namespace oryx
{

class ScriptRuntimeRegistry : public Registry<IScriptRuntime>
{
public:
    static std::vector<UniquePtr<IScriptRuntime>> create_all();
};

} // namespace oryx

#define OX_REGISTER_SCRIPT_RUNTIME(Type, name) OX_REGISTER_FACTORY(::oryx::IScriptRuntime, Type, name)
