#include "TicTacToeHeuristicStrategy.h"

#include <algorithm>

namespace {

// Duplicated from TicTacToeGame.cpp's private k_lines: two call sites don't
// justify extracting a shared utility yet (docs/design/principles.md).
constexpr size_t k_lines[8][3][2] = {
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
    for (const auto& line : k_lines)
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
    OX_PROFILE_SCOPE("TicTacToeHeuristicStrategy::decide");

    const TicTacToeState& tic_tac_toe = static_cast<const TicTacToeState&>(context.state());
    Mark my_mark = tic_tac_toe.current_player() == 0 ? Mark::X : Mark::O;
    Mark opponent_mark = (my_mark == Mark::X) ? Mark::O : Mark::X;

    const oryx::ActionId winning = find_completing_move(tic_tac_toe, my_mark);
    const oryx::ActionId blocking = find_completing_move(tic_tac_toe, opponent_mark);
    oryx::ActionList actions = tic_tac_toe.legal_actions();

    constexpr oryx::ActionId k_center = 4;
    constexpr oryx::ActionId k_corners[] = { 0, 2, 6, 8 };
    auto legal = [&](oryx::ActionId action) { return std::find(actions.begin(), actions.end(), action) != actions.end(); };
    auto rule_value = [&](oryx::ActionId action)
    {
        if (action == winning)
        {
            return 1.0;
        }
        if (action == blocking)
        {
            return 0.75;
        }
        if (action == k_center)
        {
            return 0.5;
        }
        return std::find(std::begin(k_corners), std::end(k_corners), action) != std::end(k_corners) ? 0.25 : 0.0;
    };

    oryx::ActionId chosen = oryx::INVALID_ACTION;
    if (oryx::is_valid(winning))
    {
        chosen = winning;
    }
    else if (oryx::is_valid(blocking))
    {
        chosen = blocking;
    }
    else if (legal(k_center))
    {
        chosen = k_center;
    }
    else
    {
        for (oryx::ActionId corner : k_corners)
        {
            if (chosen == oryx::INVALID_ACTION && legal(corner))
            {
                chosen = corner;
            }
        }
        if (chosen == oryx::INVALID_ACTION)
        {
            chosen = actions.front();
        }
    }

    if (oryx::IDecisionObserver* observer = context.get<oryx::IDecisionObserver>())
    {
        oryx::Decision decision;
        decision.player = tic_tac_toe.current_player();
        decision.chosen = chosen;
        for (oryx::ActionId action : actions)
        {
            oryx::set_value(decision, action, rule_value(action));
            oryx::set_probability(decision, action, action == chosen ? 1.0 : 0.0);
        }
        observer->on_decision(context.state(), decision);
    }
    return chosen;
}

} // namespace oasis

OX_REGISTER_STRATEGY(oasis::TicTacToeHeuristicStrategy, "tictactoe/heuristic", {}, "Win, else block, else center, then a corner, then anything")
