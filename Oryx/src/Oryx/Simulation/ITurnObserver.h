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
    // Called when a new match has just been created, before its first on_turn.
    virtual void on_match_start() {}
    // Called after a game action has been applied to the match.
    virtual void on_move() {}
};

} // namespace oryx
