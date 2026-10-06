#pragma once

#include "Oryx/Containers/SmallVector.h"

namespace oryx
{

using ActionId = uint32_t;

// Sized to the largest measured legal_actions() count (TicTacToe: 9, docs/design/quality.md); bigger games spill to the heap - re-measure when adding one.
constexpr size_t k_action_list_inline_capacity = 9;

using ActionList = SmallVector<ActionId, k_action_list_inline_capacity>;

constexpr ActionId INVALID_ACTION = static_cast<ActionId>(-1);

constexpr ActionId UNDO_ACTION = INVALID_ACTION - 1;

// A strategy that needs more frames (e.g. waiting on a click or on stdin) returns this; SimulationLayer retries the decision next update.
constexpr ActionId PENDING_ACTION = INVALID_ACTION - 2;

constexpr bool is_valid(ActionId action)
{ 
    return action != INVALID_ACTION; 
}

// A game's legal action IDs stay strictly below PENDING_ACTION, so the three sentinels never collide with a real move.
constexpr bool is_game_action(ActionId action)
{
    return action < PENDING_ACTION;
}

inline std::string to_string(ActionId action) 
{
    return std::to_string(action); 
}

} // namespace oryx
