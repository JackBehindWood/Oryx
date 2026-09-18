#pragma once

#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

// Exhaustive depth-first search using IState::apply()/undo() directly - no
// state cloning, no memoization/alpha-beta (Tic-Tac-Toe's branching factor
// makes full-depth search trivial; alpha-beta is Phase 12, ROADMAP.md §14).
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
