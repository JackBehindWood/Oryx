#pragma once

#include "Oryx/Core/Layer.h"
#include "Oryx/Scripting/Interfaces/IScriptRuntime.h"
#include "Oryx/Scripting/ScriptDiscovery.h"

namespace oryx
{

class ScriptingLayer : public Layer
{
public:
    explicit ScriptingLayer(ScriptDiscoveryOptions options = {});
    // Drives the runtimes without owning them (the registry owns real ones, tests own their fakes); oryx::shutdown() stops them.
    ScriptingLayer(ScriptDiscoveryOptions options, std::vector<IScriptRuntime*> runtimes);

    void attach() override;
    void event(Event& event) override;

private:
    [[nodiscard]] std::vector<ScriptSource> discover() const;
    void sync_runtimes();

    ScriptDiscoveryOptions m_options;
    std::vector<IScriptRuntime*> m_runtimes;
};

} // namespace oryx
