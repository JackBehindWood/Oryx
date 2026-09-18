#include "TicTacToeGame.h"

namespace {

// Row/col triples for the 8 winning lines: 3 rows, 3 columns, 2 diagonals.
constexpr size_t kLines[8][3][2] = {
    { { 0, 0 }, { 0, 1 }, { 0, 2 } },
    { { 1, 0 }, { 1, 1 }, { 1, 2 } },
    { { 2, 0 }, { 2, 1 }, { 2, 2 } },
    { { 0, 0 }, { 1, 0 }, { 2, 0 } },
    { { 0, 1 }, { 1, 1 }, { 2, 1 } },
    { { 0, 2 }, { 1, 2 }, { 2, 2 } },
    { { 0, 0 }, { 1, 1 }, { 2, 2 } },
    { { 0, 2 }, { 1, 1 }, { 2, 0 } },
};

} // namespace

namespace oasis
{

oryx::ActionList TicTacToeState::legal_actions() const
{
    OX_PROFILE_SCOPE("TicTacToeState::legal_actions");

    oryx::ActionList actions;
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t col = 0; col < 3; ++col)
        {
            if (m_board.at(row, col) == Mark::Empty)
            {
                actions.push_back(static_cast<oryx::ActionId>(row * 3 + col));
            }
        }
    }
    // Trips if this ever changes shape - kActionListInlineCapacity is measured from this exact board (DESIGN.md §12/§19).
    OX_CORE_ASSERT(actions.size() <= oryx::kActionListInlineCapacity, "TicTacToe legal_actions() exceeded ActionList's inline capacity");
    return actions;
}

void TicTacToeState::apply(oryx::ActionId action)
{
    size_t row = action / 3;
    size_t col = action % 3;
    m_board.at(row, col) = (m_current_player == 0) ? Mark::X : Mark::O;
    m_current_player = 1 - m_current_player;
}

void TicTacToeState::undo(oryx::ActionId action)
{
    size_t row = action / 3;
    size_t col = action % 3;
    m_board.at(row, col) = Mark::Empty;
    m_current_player = 1 - m_current_player;
}

Mark TicTacToeState::winner() const
{
    for (const auto& line : kLines)
    {
        Mark first = m_board.at(line[0][0], line[0][1]);
        if (first == Mark::Empty)
        {
            continue;
        }
        if (m_board.at(line[1][0], line[1][1]) == first && m_board.at(line[2][0], line[2][1]) == first)
        {
            return first;
        }
    }
    return Mark::Empty;
}

bool TicTacToeState::is_terminal() const
{
    return winner() != Mark::Empty || legal_actions().empty();
}

oryx::Outcome TicTacToeState::outcome() const
{
    oryx::Outcome result;
    result.is_terminal = is_terminal();
    result.rewards = oryx::Rewards<double>(2);

    Mark win = winner();
    if (win != Mark::Empty)
    {
        oryx::PlayerId winning_player = (win == Mark::X) ? 0 : 1;
        oryx::PlayerId losing_player = 1 - winning_player;
        result.rewards[winning_player] = 1.0;
        result.rewards[losing_player] = -1.0;
    }

    return result;
}

std::string TicTacToeState::action_to_string(oryx::ActionId action) const
{
    size_t row = action / 3;
    size_t col = action % 3;
    return "row " + oryx::to_string(static_cast<oryx::ActionId>(row + 1)) +
           ", col " + oryx::to_string(static_cast<oryx::ActionId>(col + 1));
}

} // namespace oasis

OX_REGISTER_GAME(oasis::TicTacToeGame, "tictactoe")
