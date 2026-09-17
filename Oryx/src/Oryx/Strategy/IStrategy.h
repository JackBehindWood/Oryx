#pragma once

#include "Oryx/Core/Registry.h"
#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

class IStrategy
{
public:
    virtual ~IStrategy() = default;

    virtual ActionId decide(const IState& state) = 0;
};

using StrategyRegistry = Registry<IStrategy>;

} // namespace oryx
