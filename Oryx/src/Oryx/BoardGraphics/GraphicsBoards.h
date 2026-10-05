#pragma once

#include "Oryx/Board/IGraphicsBoard.h"

namespace oryx
{

// The game's registered graphics board, else a PresentedGraphicsBoard over its registered presenter, else null.
// A GraphicsBoardFactory for BoardLayerDesc; `seat` is the local human's player, or kAllSeats for hot-seat.
[[nodiscard]] UniquePtr<IGraphicsBoard> create_graphics_board(const std::string& game, PlayerId seat);

} // namespace oryx
