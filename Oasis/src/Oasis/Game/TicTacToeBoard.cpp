#include "TicTacToeBoard.h"

#include <algorithm>
#include <iostream>
#include <limits>

namespace
{

char symbol(oasis::Mark mark)
{
    switch (mark)
    {
        case oasis::Mark::X: return 'X';
        case oasis::Mark::O: return 'O';
        default: return '.';
    }
}

} // namespace

namespace oasis
{

void TicTacToeBoard::print(const TicTacToeState& state) const
{
    for (size_t row = 0; row < 3; ++row)
    {
        std::cout << " " << symbol(state.mark_at(row, 0))
                   << " | " << symbol(state.mark_at(row, 1))
                   << " | " << symbol(state.mark_at(row, 2)) << "\n";
        if (row < 2)
        {
            std::cout << "---+---+---\n";
        }
    }
}

oryx::ActionId TicTacToeBoard::read_move(const TicTacToeState& state) const
{
    oryx::ActionList legal = state.legal_actions();
    char player_symbol = symbol(state.current_player() == 0 ? Mark::X : Mark::O);

    while (true)
    {
        std::cout << "Player " << player_symbol << ", enter row and col (1-3 1-3) or 'u' to undo: ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return oryx::INVALID_ACTION;
        }

        std::istringstream stream(line);
        std::string first_token;
        if (!(stream >> first_token))
        {
            continue; // Empty line
        }

        if (first_token == "u" || first_token == "undo")
        {
            return oryx::UNDO_ACTION;
        }

        try
        {
            int32_t row = std::stoi(first_token);
            int32_t col = 0;
            if (stream >> col)
            {
                oryx::ActionId action = static_cast<oryx::ActionId>((row - 1) * 3 + (col - 1));
                if (row >= 1 && row <= 3 && col >= 1 && col <= 3 &&
                    std::find(legal.begin(), legal.end(), action) != legal.end())
                {
                    return action;
                }
            }
        }
        catch (const std::exception&) {}

        std::cout << "That cell isn't available. Try again.\n";
    }
}

void TicTacToeBoard::print_outcome(const oryx::Outcome& outcome) const
{
    if (outcome.rewards[0] > outcome.rewards[1])
    {
        std::cout << "Player X wins!\n";
    }
    else if (outcome.rewards[1] > outcome.rewards[0])
    {
        std::cout << "Player O wins!\n";
    }
    else
    {
        std::cout << "It's a draw!\n";
    }
}

} // namespace oasis
