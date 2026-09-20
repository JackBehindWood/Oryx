#pragma once

#include "Oryx/Core/Layer.h"
#include "Oryx/Scripting/IScriptRuntime.h"
#include "Oryx/Scripting/ScriptDiscovery.h"

namespace oryx
{

class ScriptingLayer : public Layer
{
public:
    explicit ScriptingLayer(ScriptDiscoveryOptions options = {});
    ScriptingLayer(ScriptDiscoveryOptions options, std::vector<UniquePtr<IScriptRuntime>> runtimes);

    void attach() override;
    void detach() override;

private:
    ScriptDiscoveryOptions m_options;
    std::vector<UniquePtr<IScriptRuntime>> m_runtimes;
    std::vector<IScriptRuntime*> m_started;
};

} // namespace oryx
