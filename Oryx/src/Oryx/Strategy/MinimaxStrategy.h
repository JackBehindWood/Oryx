#pragma once

#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"
#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

// Exhaustive DFS via IState::apply()/undo(); no cloning, memoization or alpha-beta (Phase 12).
// When observed it also publishes the top plies of the search as Decision::tree: visits are the nodes searched below a node, values are the root player's.
class MinimaxStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override;

private:
    struct SearchStats
    {
        int64_t nodes = 0;
        int32_t depth_max = 0;
        PlayerId root_player = 0;
        std::vector<SearchNode>* tree = nullptr;
    };

    static constexpr int32_t k_tree_depth = 2;
    static constexpr size_t k_tree_limit = 256;

    template<bool Counting>
    ActionId choose(IState& state, Decision* decision) const;

    // Best achievable Rewards from `state` under optimal play by every
    // player, each maximizing their own reward at their own decision nodes.
    template<bool Counting>
    Rewards<double> evaluate(IState& state, SearchStats& stats, int32_t depth, int32_t node) const;

    // The tree slot for the child reached by `action` while the tree has room above k_tree_depth, else -1.
    static int32_t open_tree_node(SearchStats& stats, int32_t parent, int32_t depth, ActionId action);
    static void close_tree_node(SearchStats& stats, int32_t node, int64_t nodes_before, double value);
};

} // namespace oryx
