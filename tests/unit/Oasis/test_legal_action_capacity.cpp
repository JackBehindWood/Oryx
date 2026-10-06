#include "doctest.h"

#include "Oryx.h"
#include "Oasis/Game/HexapawnGame.h"
#include "Oasis/Game/TicTacToeGame.h"

using namespace oryx;

namespace
{

size_t widest_branching(IState& state)
{
    if (state.is_terminal())
    {
        return 0;
    }
    ActionList legal = state.legal_actions();
    size_t widest = legal.size();
    for (ActionId action : legal)
    {
        state.apply(action);
        widest = std::max(widest, widest_branching(state));
        state.undo(action);
    }
    return widest;
}

} // namespace

TEST_CASE("No reachable Tic-Tac-Toe or Hexapawn position has more legal actions than ActionList holds inline")
{
    oasis::TicTacToeState tic_tac_toe;
    oasis::HexapawnState hexapawn;

    CHECK(widest_branching(tic_tac_toe) == 9);
    CHECK(widest_branching(hexapawn) <= k_action_list_inline_capacity);
}
