#pragma once

#include "TicTacToeGame.h"

namespace oasis
{

// Shows Tic-Tac-Toe through the generic board front ends: one space per cell (action = cell index), X as a cross and O as a ring.
class TicTacToePresenter : public oryx::IBoardPresenter
{
public:
    static constexpr oryx::PieceKind k_mark = 0;

    TicTacToePresenter();

    oryx::SharedPtr<const oryx::BoardLayout> layout(const oryx::IState& state) const override;
    void describe_pieces(const oryx::IState& state, oryx::PlayerId viewer, oryx::BoardContent& out) const override;
    void action_picks(const oryx::IState& state, oryx::ActionId action, oryx::PickList& out) const override;
    oryx::PieceStyle piece_style(oryx::PieceKind kind, oryx::PlayerId owner) const override;

private:
    oryx::SharedPtr<const oryx::BoardLayout> m_layout;
};

} // namespace oasis
