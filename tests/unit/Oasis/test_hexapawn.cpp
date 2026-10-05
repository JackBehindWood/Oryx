#include "doctest.h"

#include "Oasis/Game/HexapawnGame.h"

using namespace oryx;
using namespace oasis;

namespace
{

using Direction = HexapawnState::Direction;

ActionId move(uint32_t from, Direction direction)
{
    return HexapawnState::action_for(from, direction);
}

bool legal(const IState& state, ActionId action)
{
    ActionList actions = state.legal_actions();
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}

} // namespace

TEST_CASE("Hexapawn starts with three pawns a side and only forward steps")
{
    HexapawnState state;
    CHECK(state.current_player() == 0);
    CHECK(state.owner_at(0) == 1);
    CHECK(state.owner_at(8) == 0);
    CHECK(state.owner_at(4) == HexapawnState::kEmpty);
    CHECK(state.legal_actions() == ActionList{ move(6, HexapawnState::Forward), move(7, HexapawnState::Forward), move(8, HexapawnState::Forward) });
    CHECK(state.action_to_string(move(6, HexapawnState::Forward)) == "a1-a2");
}

TEST_CASE("Hexapawn pawns capture diagonally, cannot step onto a pawn, and undo restores a capture")
{
    HexapawnState state;
    state.apply(move(7, HexapawnState::Forward));
    CHECK(state.current_player() == 1);
    CHECK_FALSE(legal(state, move(1, HexapawnState::Forward)));
    CHECK(legal(state, move(0, HexapawnState::Right)));
    CHECK(state.action_to_string(move(0, HexapawnState::Right)) == "a3xb2");

    state.apply(move(0, HexapawnState::Right));
    CHECK(state.owner_at(4) == 1);
    CHECK(state.owner_at(0) == HexapawnState::kEmpty);

    state.undo(move(0, HexapawnState::Right));
    CHECK(state.current_player() == 1);
    CHECK(state.owner_at(4) == 0);
    CHECK(state.owner_at(0) == 1);
}

TEST_CASE("Hexapawn is won by reaching the far row")
{
    HexapawnState state;
    for (ActionId action : { move(7, HexapawnState::Forward), move(0, HexapawnState::Forward), move(4, HexapawnState::Right) })
    {
        REQUIRE(legal(state, action));
        state.apply(action);
    }
    CHECK(state.is_terminal());
    CHECK(state.legal_actions().empty());
    CHECK(winner_of(state.outcome()) == 0);
}

TEST_CASE("Hexapawn is lost by the side that cannot move")
{
    HexapawnState state;
    for (ActionId action : { move(6, HexapawnState::Forward), move(1, HexapawnState::Forward), move(8, HexapawnState::Forward) })
    {
        REQUIRE(legal(state, action));
        state.apply(action);
    }
    CHECK(state.current_player() == 1);
    CHECK(state.is_terminal());
    CHECK(state.legal_actions().empty());
    CHECK(winner_of(state.outcome()) == 0);
}
