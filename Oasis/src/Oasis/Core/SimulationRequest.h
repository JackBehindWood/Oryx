#pragma once

#include "Options.h"

#include "Oryx/Events/SimulationEvent.h"

namespace oasis
{

// Builds the event that makes SimulationLayer run `--simulate=<strategyA>,<strategyB>,<matchCount>`; throws oryx::Error for anything the arguments get wrong.
[[nodiscard]] oryx::StartSimulationEvent make_simulation_request(const Options& options);

} // namespace oasis
