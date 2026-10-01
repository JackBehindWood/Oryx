#pragma once

#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"
#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

// Exhaustive DFS via IState::apply()/undo(); no cloning, memoization or alpha-beta (Phase 12).
class MinimaxStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override;

private:
    struct SearchStats
    {
        int64_t nodes = 0;
        int32_t depth_max = 0;
    };

    template<bool Counting>
    ActionId choose(IState& state, Decision* decision) const;

    // Best achievable Rewards from `state` under optimal play by every
    // player, each maximizing their own reward at their own decision nodes.
    template<bool Counting>
    Rewards<double> evaluate(IState& state, SearchStats& stats, int32_t depth) const;
};

} // namespace oryx
