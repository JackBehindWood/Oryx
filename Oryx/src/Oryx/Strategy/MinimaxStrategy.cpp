#include "MinimaxStrategy.h"

#include <limits>

#include "Oryx/Core/Registry.h"

namespace oryx
{

ActionId MinimaxStrategy::decide(const Context& context)
{
    // Context::state() yields IState& even through a const Context&; search
    // mutates via apply()/undo() below but always restores before returning.
    IState& mutable_state = context.state();
    PlayerId player = mutable_state.current_player();

    ActionId best_action = INVALID_ACTION;
    double best_value = -std::numeric_limits<double>::infinity();

    for (ActionId action : mutable_state.legal_actions())
    {
        mutable_state.apply(action);
        Rewards<double> value = evaluate(mutable_state);
        mutable_state.undo(action);

        double reward = value[player];
        if (reward > best_value)
        {
            best_value = reward;
            best_action = action;
        }
    }

    return best_action;
}

Rewards<double> MinimaxStrategy::evaluate(IState& state) const
{
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
        Rewards<double> candidate = evaluate(state);
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

OX_REGISTER_STRATEGY(oryx::MinimaxStrategy, "minimax")
