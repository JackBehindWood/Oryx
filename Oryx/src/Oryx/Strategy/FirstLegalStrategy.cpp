#include "FirstLegalStrategy.h"

#include "Oryx/Core/Registry.h"

namespace oryx
{

ActionId FirstLegalStrategy::decide(const Context& context)
{
    OX_PROFILE_SCOPE("FirstLegalStrategy::decide");

    ActionList actions = context.state().legal_actions();
    return actions.empty() ? INVALID_ACTION : actions.front();
}

} // namespace oryx

OX_REGISTER_STRATEGY(oryx::FirstLegalStrategy, "first-legal", {}, "First legal action, in the order the game lists them")
