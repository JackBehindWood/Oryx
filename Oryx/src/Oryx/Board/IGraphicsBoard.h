#pragma once

#include "Oryx/Board/BoardInput.h"
#include "Oryx/Board/IBoard.h"
#include "Oryx/Core/Registry.h"

namespace oryx
{

// A windowed board: poll_action never blocks, and BoardLayer restarts the game after it ends until the window closes.
// It reads input only through the BoardInput BoardLayer hands it, so it runs and tests without a window.
class IGraphicsBoard : public IBoard
{
public:
    // Called once per frame by the owning layer, first: reacts to the frame's input and queues any move for poll_action.
    virtual void update(const BoardInput& input, double delta_time) = 0;
    // Called once per frame after update; draws from what on_turn captured.
    virtual void render(const BoardInput& input) = 0;
};

using GraphicsBoardRegistry = Registry<IGraphicsBoard>;

// Makes the windowed board for a game and seat (a player, or kAllSeats for hot-seat). The application hands one to BoardLayer because
// the generic presented board draws, so it lives above this headless module (BoardGraphics/create_graphics_board).
using GraphicsBoardFactory = std::function<UniquePtr<IGraphicsBoard>(const std::string& game, PlayerId seat)>;

} // namespace oryx

#define OX_REGISTER_GRAPHICS_BOARD(Type, game) \
    OX_REGISTER_FACTORY(::oryx::IGraphicsBoard, Type, game)
