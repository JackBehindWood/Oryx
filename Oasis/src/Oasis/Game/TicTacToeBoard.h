#pragma once

#include "TicTacToeGame.h"

namespace oasis
{

// Concrete stdin/stdout renderer for TicTacToeState — not behind an
// interface, since with only one game and one renderer there is nothing
// yet to abstract over (see DESIGN.md §16 / ARCHITECTURE.md §8).
constexpr oryx::ActionId UNDO_ACTION = oryx::INVALID_ACTION - 1;

class TicTacToeBoard
{
public:
    void print(const TicTacToeState& state) const;

    // Returns oryx::INVALID_ACTION if stdin is closed/exhausted (e.g.
    // Ctrl+D or a redirected input runs out) instead of prompting forever.
    oryx::ActionId read_move(const TicTacToeState& state) const;

    void print_outcome(const oryx::Outcome& outcome) const;
};

} // namespace oasis
