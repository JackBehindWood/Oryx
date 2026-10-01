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
};

} // namespace oryx
