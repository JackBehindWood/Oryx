#include "RandomStrategy.h"

#include "Oryx/Core/Registry.h"
#include "Oryx/Strategy/Observability/IDecisionObserver.h"

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
    ActionId chosen = actions[static_cast<size_t>(index)];

    if (IDecisionObserver* observer = context.get<IDecisionObserver>())
    {
        Decision decision;
        decision.player = context.state().current_player();
        decision.chosen = chosen;
        double probability = 1.0 / static_cast<double>(actions.size());
        for (ActionId action : actions)
        {
            set_probability(decision, action, probability);
        }
        add_metric(decision.extra, "random/legal_actions", static_cast<double>(actions.size()));
        observer->on_decision(context.state(), decision);
    }
    return chosen;
}

} // namespace oryx

OX_REGISTER_STRATEGY(oryx::RandomStrategy, "random",
    { oryx::param_without_default("seed", oryx::ParamType::Int, "Seed for the generator; omit for a non-deterministic one") },
    "Uniformly random legal action")
