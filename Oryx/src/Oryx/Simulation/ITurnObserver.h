#pragma once

#include "Oryx/Game/IState.h"

namespace oryx
{

// Watches a running simulation; SimulationLayer calls it every update, before the decision.
class ITurnObserver
{
public:
    virtual ~ITurnObserver() = default;

    virtual void on_turn(IState& state) = 0;
};

} // namespace oryx
