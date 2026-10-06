#pragma once

#include "Oryx/Board/Graphics/IGraphicsBoard.h"
#include "Oryx/Board/IBoardPresenter.h"

namespace oryx
{

namespace selection
{

enum class FrontEnd
{
    Console,
    Graphical
};

// Graphical only when graphics are built and wanted and the game has a graphics board or a presenter; otherwise Console.
// A graphical choice resolves the game now (defaulting it, never prompting); a console choice keeps `requested`, which BoardLayer may still prompt for. False means exit.
[[nodiscard]] bool choose_front_end(const std::string& requested_game, bool headless, bool graphics_built, FrontEnd& out_front_end, std::string& out_game);

} // namespace selection

// The game's registered graphics board, else a PresentedGraphicsBoard2D over its registered presenter, else null.
// `seat` is the local human's player, or k_all_seats for hot-seat.
[[nodiscard]] UniquePtr<IGraphicsBoard> create_graphics_board(const std::string& game, PlayerId seat);

} // namespace oryx
