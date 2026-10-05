#pragma once

#include "Oryx/Board/IBoard.h"
#include "Oryx/Core/Registry.h"

namespace oryx
{

// BoardLayer restarts a finished game on this input; boards that show the finished state say so with this text.
constexpr const char* kRestartHint = "click or press R to play again";

// A windowed board: poll_action never blocks, and BoardLayer restarts the game after it ends until the window closes.
class IGraphicsBoard : public IBoard
{
public:
    // Called once per frame by the owning layer; draws from what on_turn captured.
    virtual void render(double delta_time) = 0;
};

using GraphicsBoardRegistry = Registry<IGraphicsBoard>;

} // namespace oryx

#define OX_REGISTER_GRAPHICS_BOARD(Type, game) \
    OX_REGISTER_FACTORY(::oryx::IGraphicsBoard, Type, game)
