#pragma once

#include "Oryx/Game/IState.h"
#include "Oryx/Strategy/Observability/Decision.h"

namespace oryx
{

// Provided through Context::provide<IDecisionObserver>(); never in required_capabilities().
class IDecisionObserver
{
public:
    virtual ~IDecisionObserver() = default;

    virtual void on_decision(const IState& state, const Decision& decision) = 0;
    // A new match is about to be decided; decisions that follow belong to it.
    virtual void on_match_start() {}
};

} // namespace oryx
