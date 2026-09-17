#include "OasisLayer.h"

namespace oasis
{

OasisLayer::OasisLayer()
    : oryx::Layer("OasisLayer")
{
}

void OasisLayer::attach()
{
    m_game = oryx::GameRegistry::create("tictactoe");
    if (!m_game)
    {
        OX_ERROR("Failed to create game 'tictactoe' — is it registered?");
        oryx::Application::Get().close();
        return;
    }

    m_state = m_game->new_initial_state();
}

void OasisLayer::update()
{
    if (!m_state)
    {
        return;
    }

    TicTacToeState& state = static_cast<TicTacToeState&>(*m_state);
    m_board.print(state);

    if (state.is_terminal())
    {
        m_board.print_outcome(state.outcome());
        oryx::Application::Get().close();
        return;
    }

    ActionId action = m_board.read_move(state);

    if (!oryx::is_valid(action))
    {
        OX_INFO("Input closed before the game finished — exiting.");
        oryx::Application::Get().close();
        return;
    }

    if (action == UNDO_ACTION)
    {
        if (m_history.empty())
        {
            OX_WARN("No moves to undo.");
            return;
        }
        ActionId last_action = m_history.back();
        m_history.pop_back();
        state.undo(last_action);
        return;
    }

    state.apply(action);
    m_history.push_back(action);
}

void OasisLayer::detach()
{
}

} // namespace oasis
