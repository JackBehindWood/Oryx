#include "BoardSession.h"

#include "Oryx/Board/ConsoleGame.h"

namespace oryx
{

BoardSession::BoardSession(SharedPtr<IBoard> board, bool announce_outcome)
    : m_board(std::move(board))
    , m_announce_outcome(announce_outcome)
{
}

void BoardSession::on_turn(IState& state)
{
    m_board->on_turn(state);

    if (!state.is_terminal() || m_game_over)
    {
        return;
    }

    m_game_over = true;
    if (m_announce_outcome)
    {
        print_console_outcome(state.outcome());
    }
}

ActionId BoardSession::next_action(const Context& context)
{
    return m_board->poll_action(context.state());
}

void BoardSession::advance(double delta_time)
{
    if (m_game_over)
    {
        m_seconds_over += delta_time;
    }
}

void BoardSession::restart()
{
    m_game_over = false;
    m_seconds_over = 0.0;
}

} // namespace oryx
