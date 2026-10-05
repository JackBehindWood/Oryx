#pragma once

#include "TicTacToeGame.h"

namespace oasis
{

// Shows Tic-Tac-Toe through the generic board front ends: one space per cell (action = cell index), X as a cross and O as a ring.
class TicTacToePresenter : public oryx::IBoardPresenter
{
public:
    static constexpr oryx::PieceKind kMark = 0;

    void describe(const oryx::IState& state, oryx::PlayerId viewer, oryx::BoardView& out) const override;
    void action_picks(const oryx::IState& state, oryx::ActionId action, oryx::PickList& out) const override;
    oryx::PieceStyle piece_style(oryx::PieceKind kind, oryx::PlayerId owner) const override;
};

} // namespace oasis
