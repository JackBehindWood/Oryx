#pragma once

#include "Oryx/Core/Registry.h"
#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/Context.h"


namespace oryx
{

class IStrategy
{
public:
    virtual ~IStrategy() = default;

    virtual ActionId decide(const Context& context) = 0;

    // Capabilities this strategy needs the Context to provide (see
    // Oryx/Game/Context.h). Empty by default.
    [[nodiscard]] virtual std::vector<std::type_index> required_capabilities() const { return {}; }
};

using StrategyRegistry = Registry<IStrategy>;

inline UniquePtr<IStrategy> create_strategy(const std::string& name)
{
    return StrategyRegistry::create(name);
}

} // namespace oryx
