#pragma once

#include "Oasis/Game/TicTacToeGame.h"

namespace oasis
{

// Rule-of-thumb strategy, not a search: win if possible, else block, else
// prefer center, then a corner, then whatever's left. MinimaxStrategy
// (Oryx/Strategy) already covers the unbeatable case.
class TicTacToeHeuristicStrategy : public oryx::IStrategy
{
public:
    oryx::ActionId decide(const oryx::Context& context) override;
};

} // namespace oasis
