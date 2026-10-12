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

int32_t MinimaxStrategy::open_tree_node(SearchStats& stats, int32_t parent, int32_t depth, ActionId action)
{
    if (stats.tree == nullptr || parent < 0 || depth >= k_tree_depth || stats.tree->size() >= k_tree_limit)
    {
        return -1;
    }
    stats.tree->push_back(SearchNode{ parent, action, 0, 0.0 });
    return static_cast<int32_t>(stats.tree->size() - 1);
}

void MinimaxStrategy::close_tree_node(SearchStats& stats, int32_t node, int64_t nodes_before, double value)
{
    if (node >= 0)
    {
        SearchNode& slot = (*stats.tree)[static_cast<size_t>(node)];
        slot.visits = stats.nodes - nodes_before;
        slot.value = value;
    }
}

template<bool Counting>
ActionId MinimaxStrategy::choose(IState& state, Decision* decision) const
{
    PlayerId player = state.current_player();

    ActionId best_action = INVALID_ACTION;
    double best_value = -std::numeric_limits<double>::infinity();
    SearchStats stats;
    stats.root_player = player;
    int32_t root = -1;
    if constexpr (Counting)
    {
        stats.tree = &decision->tree;
        decision->tree.push_back(SearchNode{ -1, INVALID_ACTION, 0, 0.0 });
        root = 0;
    }

    for (ActionId action : state.legal_actions())
    {
        const int64_t before = stats.nodes;
        const int32_t child = open_tree_node(stats, root, 0, action);
        state.apply(action);
        Rewards<double> value = evaluate<Counting>(state, stats, 1, child);
        state.undo(action);

        double reward = value[player];
        close_tree_node(stats, child, before, reward);
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
        close_tree_node(stats, root, 0, best_value);
        size_t optimal = 0;
        for (const ActionScore& score : decision->scores)
        {
            optimal += score.value == best_value ? 1 : 0;
        }
        for (size_t i = 0; i < decision->scores.size(); ++i)
        {
            const ActionId scored = decision->scores[i].action;
            set_probability(*decision, scored, decision->scores[i].value == best_value ? 1.0 / static_cast<double>(optimal) : 0.0);
        }
        add_metric(decision->extra, "minimax/nodes", static_cast<double>(stats.nodes));
        max_metric(decision->extra, "minimax/depth_max", static_cast<double>(stats.depth_max));
    }
    return best_action;
}

template<bool Counting>
Rewards<double> MinimaxStrategy::evaluate(IState& state, SearchStats& stats, int32_t depth, int32_t node) const
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
        const int64_t before = stats.nodes;
        const int32_t child = open_tree_node(stats, node, depth, action);
        state.apply(action);
        Rewards<double> candidate = evaluate<Counting>(state, stats, depth + 1, child);
        state.undo(action);
        close_tree_node(stats, child, before, candidate[stats.root_player]);

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
