#pragma once

#include "Oryx.h"

namespace oasis
{

class OasisApp : public oryx::Application
{
public:
    explicit OasisApp(oryx::ApplicationCommandLineArgs args);

protected:
    void on_event(oryx::Event& event) override;
    void on_layer_disabled(oryx::Layer& layer, std::string_view phase) override;

private:
    bool on_start_simulation(oryx::StartSimulationEvent& event);
};

} // namespace oasis
