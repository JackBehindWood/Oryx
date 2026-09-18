#include "RandomStrategy.h"

#include "Oryx/Core/Registry.h"

namespace oryx
{

ActionId RandomStrategy::decide(const Context& context)
{
    OX_PROFILE_SCOPE("RandomStrategy::decide");

    ActionList actions = context.state().legal_actions();
    int64_t index = m_random.get_int(0, static_cast<int64_t>(actions.size()) - 1);
    return actions[static_cast<size_t>(index)];
}

} // namespace oryx

OX_REGISTER_STRATEGY(oryx::RandomStrategy, "random")
