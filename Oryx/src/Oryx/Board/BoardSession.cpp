#include "BoardSession.h"

#include "Oryx/Board/Console/ConsoleGame.h"

namespace oryx
{

BoardSession::BoardSession(SharedPtr<IBoard> board, bool announce_outcome, PlayerId seat)
    : m_board(std::move(board))
    , m_announce_outcome(announce_outcome)
    , m_next_seat(seat)
{
}

void BoardSession::on_turn(IState& state)
{
    m_board->on_turn(state);

    if (!state.is_terminal() || m_game_over || m_restarting)
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

void BoardSession::restart(PlayerId seat)
{
    m_next_seat = seat;
    m_restarting = true;
    m_game_over = false;
    m_seconds_over = 0.0;
}

void BoardSession::on_match_start()
{
    m_restarting = false;
    m_game_over = false;
    m_seconds_over = 0.0;
    m_moves = 0;
    m_board->reset(m_next_seat);
}

} // namespace oryx
