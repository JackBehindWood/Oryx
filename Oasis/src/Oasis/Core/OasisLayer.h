#pragma once

#include "Options.h"

namespace oasis
{

// Everything specific to Oasis: decides how it starts (simulation, windowed or terminal play) and reports simulation results.
class OasisLayer : public oryx::Layer
{
public:
    explicit OasisLayer(Options options);

    void attach() override;
    void event(oryx::Event& event) override;

private:
    void start_simulation();
    void start_play(const LaunchPlan& plan);
    // False leaves the application without a window or renderer so the caller can fall back to the terminal.
    bool start_graphics(const LaunchPlan& plan);
    bool on_simulation_complete(oryx::SimulationCompleteEvent& event);

    Options m_options;
};

} // namespace oasis
