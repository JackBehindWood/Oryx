#pragma once

#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

// Exhaustive DFS via IState::apply()/undo(); no cloning, memoization or alpha-beta (Phase 12).
class MinimaxStrategy : public IStrategy
{
public:
    ActionId decide(const Context& context) override;

private:
    // Best achievable Rewards from `state` under optimal play by every
    // player, each maximizing their own reward at their own decision nodes.
    Rewards<double> evaluate(IState& state) const;
};

} // namespace oryx
