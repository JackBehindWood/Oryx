#include "ConsoleBoard.h"

#include "ConsoleGame.h"

namespace oryx
{

void ConsoleBoard::on_turn(const IState&)
{
}

ActionId ConsoleBoard::poll_action(const IState& state)
{
    return read_console_move(state);
}

UniquePtr<IConsoleBoard> create_console_board(const std::string& game)
{
    UniquePtr<IConsoleBoard> board = ConsoleBoardRegistry::create(game);
    return board ? std::move(board) : create_unique<ConsoleBoard>();
}

} // namespace oryx
