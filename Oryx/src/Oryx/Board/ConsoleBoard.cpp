#include "ConsoleBoard.h"

#include "ConsoleGame.h"
#include "PresentedConsoleBoard.h"

namespace oryx
{

void ConsoleBoard::on_turn(const IState&)
{
}

ActionId ConsoleBoard::poll_action(const IState& state)
{
    return read_console_move(state);
}

UniquePtr<IConsoleBoard> create_console_board(const std::string& game, PlayerId seat)
{
    if (UniquePtr<IConsoleBoard> board = ConsoleBoardRegistry::create(game))
    {
        return board;
    }
    if (UniquePtr<IBoardPresenter> presenter = BoardPresenterRegistry::create(game))
    {
        return create_unique<PresentedConsoleBoard>(std::move(presenter), game, seat);
    }
    return create_unique<ConsoleBoard>();
}

} // namespace oryx
