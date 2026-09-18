#pragma once

#include "Oryx/Containers/SmallVector.h"

namespace oryx
{

using ActionId = uint32_t;

// Sized to the largest legal_actions() count measured across registered games (TicTacToe: 9,
// DESIGN.md §12/§19); a game with a bigger branching factor still works but silently loses the
// inline fast path. Re-measure, don't guess, if/when one is added.
constexpr size_t kActionListInlineCapacity = 9;

using ActionList = SmallVector<ActionId, kActionListInlineCapacity>;

constexpr ActionId INVALID_ACTION = static_cast<ActionId>(-1);

constexpr ActionId UNDO_ACTION = INVALID_ACTION - 1;

constexpr bool is_valid(ActionId action)
{ 
    return action != INVALID_ACTION; 
}

inline std::string to_string(ActionId action) 
{
    return std::to_string(action); 
}

} // namespace oryx
