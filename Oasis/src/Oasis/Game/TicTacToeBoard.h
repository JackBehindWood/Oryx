#pragma once

#include "TicTacToeGame.h"

namespace oasis
{

// Concrete stdin/stdout renderer; not behind an interface until a second renderer exists (docs/design/platform.md).
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
