#include "TicTacToeConsoleBoard.h"

namespace oasis
{

void TicTacToeConsoleBoard::on_turn(const oryx::IState& state)
{
    m_model.update(state);
    print();
}

oryx::ActionId TicTacToeConsoleBoard::poll_action(const oryx::IState& state)
{
    m_model.update(state);
    return read_move();
}

void TicTacToeConsoleBoard::print() const
{
    for (size_t row = 0; row < TicTacToeBoardModel::kSize; ++row)
    {
        std::cout << " " << TicTacToeBoardModel::symbol_of(m_model.mark_at(row, 0))
                   << " | " << TicTacToeBoardModel::symbol_of(m_model.mark_at(row, 1))
                   << " | " << TicTacToeBoardModel::symbol_of(m_model.mark_at(row, 2)) << "\n";
        if (row + 1 < TicTacToeBoardModel::kSize)
        {
            std::cout << "---+---+---\n";
        }
    }
}

oryx::ActionId TicTacToeConsoleBoard::read_move() const
{
    char player_symbol = TicTacToeBoardModel::symbol_of(TicTacToeBoardModel::mark_of(m_model.current_player()));

    while (std::cin)
    {
        std::cout << "Player " << player_symbol << ", enter row and col (1-3 1-3) or 'u' to undo: ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            break;
        }

        std::istringstream stream(line);
        std::string first_token;
        if (!(stream >> first_token))
        {
            continue;
        }

        if (first_token == "u" || first_token == "undo")
        {
            return oryx::UNDO_ACTION;
        }

        try
        {
            int32_t row = std::stoi(first_token);
            int32_t col = 0;
            if (stream >> col && row >= 1 && row <= 3 && col >= 1 && col <= 3 &&
                m_model.legal(static_cast<size_t>(row - 1), static_cast<size_t>(col - 1)))
            {
                return TicTacToeBoardModel::action_for(static_cast<size_t>(row - 1), static_cast<size_t>(col - 1));
            }
        }
        catch (const std::exception&) {}

        std::cout << "That cell isn't available. Try again.\n";
    }
    return oryx::PENDING_ACTION;
}

OX_REGISTER_CONSOLE_BOARD(TicTacToeConsoleBoard, "tictactoe")

} // namespace oasis
