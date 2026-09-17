#include "OasisLayer.h"

#include "Oasis/Game/TicTacToeBoard.h"
#include "Oasis/Game/TicTacToeGame.h"

namespace oasis
{

OasisLayer::OasisLayer()
    : oryx::Layer("OasisLayer")
{
}

void OasisLayer::attach()
{
    TicTacToeGame game;
    TicTacToeBoard board;
    oryx::UniquePtr<oryx::IState> owned_state = game.new_initial_state();
    TicTacToeState& state = static_cast<TicTacToeState&>(*owned_state);

    std::vector<oryx::ActionId> history;

    while (!state.is_terminal())
    {
        board.print(state);
        oryx::ActionId action = board.read_move(state);

        if (!oryx::is_valid(action))
        {
            OX_INFO("Input closed before the game finished — exiting.");
            oryx::Application::Get().close();
            return;
        }

        if (action == UNDO_ACTION)
        {
            if (history.empty())
            {
                OX_WARN("No moves to undo.");
                continue;
            }
            oryx::ActionId last_action = history.back();
            history.pop_back();
            state.undo(last_action);
            continue;
        }
        state.apply(action);
        history.push_back(action);
    }

    board.print(state);
    board.print_outcome(state.outcome());

    oryx::Application::Get().close();
}

} // namespace oasis
