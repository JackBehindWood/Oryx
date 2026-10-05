#pragma once

#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

// Where an ExternalStrategy gets its moves from (a human at a board, a script, a network peer).
class IActionSource
{
public:
    virtual ~IActionSource() = default;

    // The move for the current seat, UNDO_ACTION to take a move back, or PENDING_ACTION while none is chosen yet.
    virtual ActionId next_action(const Context& context) = 0;
};

} // namespace oryx
