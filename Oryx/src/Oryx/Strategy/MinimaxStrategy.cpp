#include "MinimaxStrategy.h"

#include <limits>

#include "Oryx/Core/Registry.h"

namespace oryx
{

ActionId MinimaxStrategy::decide(const Context& context)
{
    OX_PROFILE_SCOPE("MinimaxStrategy::decide");

    // Context::state() yields IState& even through a const Context&; search
    // mutates via apply()/undo() below but always restores before returning.
    IState& mutable_state = context.state();

    IDecisionObserver* observer = context.get<IDecisionObserver>();
    if (observer == nullptr)
    {
        return choose<false>(mutable_state, nullptr);
    }

    Decision decision;
    ActionId action = choose<true>(mutable_state, &decision);
    observer->on_decision(mutable_state, decision);
    return action;
}

template<bool Counting>
ActionId MinimaxStrategy::choose(IState& state, Decision* decision) const
{
    PlayerId player = state.current_player();

    ActionId best_action = INVALID_ACTION;
    double best_value = -std::numeric_limits<double>::infinity();
    SearchStats stats;

    for (ActionId action : state.legal_actions())
    {
        state.apply(action);
        Rewards<double> value = evaluate<Counting>(state, stats, 1);
        state.undo(action);

        double reward = value[player];
        if constexpr (Counting)
        {
            set_value(*decision, action, reward);
        }
        if (reward > best_value)
        {
            best_value = reward;
            best_action = action;
        }
    }

    if constexpr (Counting)
    {
        decision->player = player;
        decision->chosen = best_action;
        add_metric(decision->extra, "minimax/nodes", static_cast<double>(stats.nodes));
        max_metric(decision->extra, "minimax/depth_max", static_cast<double>(stats.depth_max));
    }
    return best_action;
}

template<bool Counting>
Rewards<double> MinimaxStrategy::evaluate(IState& state, SearchStats& stats, int32_t depth) const
{
    if constexpr (Counting)
    {
        ++stats.nodes;
        stats.depth_max = std::max(stats.depth_max, depth);
    }

    if (state.is_terminal())
    {
        return state.outcome().rewards;
    }

    PlayerId player = state.current_player();

    Rewards<double> best(0);
    bool have_best = false;

    for (ActionId action : state.legal_actions())
    {
        state.apply(action);
        Rewards<double> candidate = evaluate<Counting>(state, stats, depth + 1);
        state.undo(action);

        if (!have_best || candidate[player] > best[player])
        {
            best = candidate;
            have_best = true;
        }
    }

    return best;
}

} // namespace oryx

OX_REGISTER_STRATEGY(oryx::MinimaxStrategy, "minimax", {}, "Exhaustive depth-first search via apply()/undo()")
