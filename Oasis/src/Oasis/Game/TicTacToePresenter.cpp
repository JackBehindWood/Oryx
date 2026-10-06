#include "TicTacToePresenter.h"

namespace oasis
{

namespace
{

constexpr uint32_t k_size = 3;

const char* mark_name(oryx::PlayerId player)
{
    return player == 0 ? "X" : "O";
}

} // namespace

TicTacToePresenter::TicTacToePresenter()
    : m_layout(oryx::make_grid_layout(k_size, k_size, false))
{
}

oryx::SharedPtr<const oryx::BoardLayout> TicTacToePresenter::layout(const oryx::IState&) const
{
    return m_layout;
}

void TicTacToePresenter::describe_pieces(const oryx::IState& state, oryx::PlayerId, oryx::BoardContent& out) const
{
    const TicTacToeState& board = static_cast<const TicTacToeState&>(state);

    for (uint32_t row = 0; row < k_size; ++row)
    {
        for (uint32_t col = 0; col < k_size; ++col)
        {
            Mark mark = board.mark_at(row, col);
            if (mark != Mark::Empty)
            {
                out.pieces.push_back({ k_mark, mark == Mark::X ? 0 : 1, row * k_size + col });
            }
        }
    }

    if (!board.is_terminal())
    {
        out.status = std::string(mark_name(board.current_player())) + " to move";
        return;
    }
    int32_t winner = oryx::winner_of(board.outcome());
    out.status = winner < 0 ? "Draw" : std::string(mark_name(winner)) + " wins";
}

void TicTacToePresenter::action_picks(const oryx::IState&, oryx::ActionId action, oryx::PickList& out) const
{
    out.push_back({ oryx::PickKind::Space, action, {} });
}

oryx::PieceStyle TicTacToePresenter::piece_style(oryx::PieceKind, oryx::PlayerId owner) const
{
    if (owner == 0)
    {
        return { "X", { 0.95f, 0.45f, 0.35f, 1.0f }, oryx::PieceShape::Cross };
    }
    return { "O", { 0.35f, 0.70f, 0.95f, 1.0f }, oryx::PieceShape::Ring };
}

OX_REGISTER_BOARD_PRESENTER(TicTacToePresenter, "tictactoe")

} // namespace oasis
