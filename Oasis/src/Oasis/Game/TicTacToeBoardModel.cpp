#include "TicTacToeBoardModel.h"

namespace oasis
{

void TicTacToeBoardModel::update(const oryx::IState& state)
{
    const TicTacToeState& tic_tac_toe = static_cast<const TicTacToeState&>(state);
    for (size_t row = 0; row < kSize; ++row)
    {
        for (size_t col = 0; col < kSize; ++col)
        {
            m_marks[row][col] = tic_tac_toe.mark_at(row, col);
        }
    }
    m_legal = tic_tac_toe.legal_actions();
    m_current_player = tic_tac_toe.current_player();
    m_terminal = tic_tac_toe.is_terminal();
    m_winner = m_terminal ? oryx::winner_of(tic_tac_toe.outcome()) : -1;
}

bool TicTacToeBoardModel::legal(size_t row, size_t col) const
{
    if (row >= kSize || col >= kSize)
    {
        return false;
    }
    return std::find(m_legal.begin(), m_legal.end(), action_for(row, col)) != m_legal.end();
}

std::string TicTacToeBoardModel::status_text() const
{
    if (!m_terminal)
    {
        return std::string(1, symbol_of(mark_of(m_current_player))) + " to move";
    }
    if (m_winner < 0)
    {
        return "Draw";
    }
    return std::string(1, symbol_of(mark_of(m_winner))) + " wins";
}

char TicTacToeBoardModel::symbol_of(Mark mark)
{
    switch (mark)
    {
        case Mark::X: return 'X';
        case Mark::O: return 'O';
        default: return '.';
    }
}

} // namespace oasis
