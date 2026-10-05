#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

// How one game is shown to and played by a human; a front-end layer drives it and knows nothing about the game behind it.
class IBoard
{
public:
    virtual ~IBoard() = default;

    // Called every simulation update before the decision, including on the finished state.
    virtual void on_turn(const IState& state) = 0;
    // The human's move for the current seat, UNDO_ACTION to take one back, or PENDING_ACTION while none is chosen yet.
    virtual ActionId poll_action(const IState& state) = 0;
    // True when the board itself shows what every seat played, so opponents need not announce their moves.
    [[nodiscard]] virtual bool shows_moves() const = 0;
};

} // namespace oryx
