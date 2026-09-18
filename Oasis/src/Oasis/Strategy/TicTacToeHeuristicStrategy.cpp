#include "TicTacToeHeuristicStrategy.h"

#include <algorithm>

namespace {

// Duplicated from TicTacToeGame.cpp's private kLines: two call sites don't
// justify extracting a shared utility yet (DESIGN.md §20).
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

// Returns the cell that completes a line for `mark` (two of `mark`, one
// empty), or INVALID_ACTION if no such line exists.
oryx::ActionId find_completing_move(const oasis::TicTacToeState& state, oasis::Mark mark)
{
    for (const auto& line : kLines)
    {
        int32_t mark_count = 0;
        int32_t empty_index = -1;
        for (int32_t i = 0; i < 3; ++i)
        {
            oasis::Mark cell = state.mark_at(line[i][0], line[i][1]);
            if (cell == mark)
            {
                ++mark_count;
            }
            else if (cell == oasis::Mark::Empty)
            {
                empty_index = i;
            }
        }
        if (mark_count == 2 && empty_index != -1)
        {
            size_t row = line[empty_index][0];
            size_t col = line[empty_index][1];
            return static_cast<oryx::ActionId>(row * 3 + col);
        }
    }
    return oryx::INVALID_ACTION;
}

} // namespace

namespace oasis
{

oryx::ActionId TicTacToeHeuristicStrategy::decide(const oryx::Context& context)
{
    const TicTacToeState& tic_tac_toe = static_cast<const TicTacToeState&>(context.state());
    Mark my_mark = tic_tac_toe.current_player() == 0 ? Mark::X : Mark::O;
    Mark opponent_mark = (my_mark == Mark::X) ? Mark::O : Mark::X;

    oryx::ActionId winning = find_completing_move(tic_tac_toe, my_mark);
    if (oryx::is_valid(winning))
    {
        return winning;
    }

    oryx::ActionId blocking = find_completing_move(tic_tac_toe, opponent_mark);
    if (oryx::is_valid(blocking))
    {
        return blocking;
    }

    oryx::ActionList actions = tic_tac_toe.legal_actions();

    constexpr oryx::ActionId kCenter = 4;
    if (std::find(actions.begin(), actions.end(), kCenter) != actions.end())
    {
        return kCenter;
    }

    constexpr oryx::ActionId kCorners[] = { 0, 2, 6, 8 };
    for (oryx::ActionId corner : kCorners)
    {
        if (std::find(actions.begin(), actions.end(), corner) != actions.end())
        {
            return corner;
        }
    }

    return actions.front();
}

} // namespace oasis

OX_REGISTER_STRATEGY(oasis::TicTacToeHeuristicStrategy, "tictactoe/heuristic")
