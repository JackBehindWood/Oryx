#include "HexapawnPresenter.h"

namespace oasis
{

namespace
{

constexpr uint32_t k_size = HexapawnState::k_size;

const char* side_name(oryx::PlayerId player)
{
    return player == 0 ? "White" : "Black";
}

} // namespace

void HexapawnPresenter::describe(const oryx::IState& state, oryx::PlayerId, oryx::BoardView& out) const
{
    const HexapawnState& board = static_cast<const HexapawnState&>(state);
    oryx::grid_spaces(k_size, k_size, true, out);

    for (uint32_t square = 0; square < k_size * k_size; ++square)
    {
        int8_t owner = board.owner_at(square);
        if (owner != HexapawnState::k_empty)
        {
            out.pieces.push_back({ k_pawn, owner, square });
        }
    }

    if (!board.is_terminal())
    {
        out.status = std::string(side_name(board.current_player())) + " to move";
        return;
    }
    out.status = std::string(side_name(oryx::winner_of(board.outcome()))) + " wins";
}

void HexapawnPresenter::action_picks(const oryx::IState& state, oryx::ActionId action, oryx::PickList& out) const
{
    out.push_back({ oryx::PickKind::Space, HexapawnState::from_square(action), {} });
    out.push_back({ oryx::PickKind::Space, HexapawnState::to_square(action, state.current_player()), {} });
}

oryx::PieceStyle HexapawnPresenter::piece_style(oryx::PieceKind, oryx::PlayerId owner) const
{
    if (owner == 0)
    {
        return { "W", { 0.94f, 0.92f, 0.86f, 1.0f }, oryx::PieceShape::Disc };
    }
    return { "B", { 0.42f, 0.45f, 0.56f, 1.0f }, oryx::PieceShape::Disc };
}

OX_REGISTER_BOARD_PRESENTER(HexapawnPresenter, "hexapawn")

} // namespace oasis
