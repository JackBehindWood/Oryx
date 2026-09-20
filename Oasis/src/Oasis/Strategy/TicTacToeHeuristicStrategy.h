#pragma once

#include "Oasis/Game/TicTacToeGame.h"

namespace oasis
{

// Rule of thumb, not a search: win, else block, else center, then a corner, then anything.
class TicTacToeHeuristicStrategy : public oryx::IStrategy
{
public:
    oryx::ActionId decide(const oryx::Context& context) override;
};

} // namespace oasis
