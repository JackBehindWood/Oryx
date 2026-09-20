#pragma once

#include "Oryx/Containers/SmallVector.h"

namespace oryx
{

using ActionId = uint32_t;

// Sized to the largest measured legal_actions() count (TicTacToe: 9, docs/design/quality.md); bigger games spill to the heap - re-measure when adding one.
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
