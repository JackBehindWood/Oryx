#include "RandomStrategy.h"

#include "Oryx/Core/Registry.h"

namespace oryx
{

RandomStrategy::RandomStrategy(const Params& params)
{
    if (has_param(params, "seed"))
    {
        m_random.seed(static_cast<uint64_t>(get_param<int64_t>(params, "seed")));
    }
}

ActionId RandomStrategy::decide(const Context& context)
{
    OX_PROFILE_SCOPE("RandomStrategy::decide");

    ActionList actions = context.state().legal_actions();
    if (actions.empty())
    {
        return INVALID_ACTION;
    }
    int64_t index = m_random.get_int(0, static_cast<int64_t>(actions.size()) - 1);
    return actions[static_cast<size_t>(index)];
}

} // namespace oryx

OX_REGISTER_STRATEGY(oryx::RandomStrategy, "random",
    { oryx::param_without_default("seed", oryx::ParamType::Int, "Seed for the generator; omit for a non-deterministic one") },
    "Uniformly random legal action")
