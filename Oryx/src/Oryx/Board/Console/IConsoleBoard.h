#pragma once

#include "Oryx/Board/IBoard.h"
#include "Oryx/Core/Registry.h"

namespace oryx
{

// A terminal board: poll_action may block on stdin and returns PENDING_ACTION once stdin is exhausted, and the run ends after one game.
class IConsoleBoard : public IBoard
{
};

using ConsoleBoardRegistry = Registry<IConsoleBoard>;

} // namespace oryx

// `game` is the registered game name the board presents.
#define OX_REGISTER_CONSOLE_BOARD(Type, game) \
    OX_REGISTER_FACTORY(::oryx::IConsoleBoard, Type, game)
