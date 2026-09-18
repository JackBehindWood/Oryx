#include "FirstLegalStrategy.h"

#include "Oryx/Core/Registry.h"

namespace oryx
{

ActionId FirstLegalStrategy::decide(const Context& context)
{
    ActionList actions = context.state().legal_actions();
    return actions.empty() ? INVALID_ACTION : actions.front();
}

} // namespace oryx

OX_REGISTER_STRATEGY(oryx::FirstLegalStrategy, "first-legal")
