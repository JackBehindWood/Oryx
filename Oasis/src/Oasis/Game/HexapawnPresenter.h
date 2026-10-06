#pragma once

#include "HexapawnGame.h"

namespace oasis
{

// Shows Hexapawn through the generic board front ends: a checkered 3x3 board, and a move picked as the pawn, then its target square.
class HexapawnPresenter : public oryx::IBoardPresenter
{
public:
    static constexpr oryx::PieceKind k_pawn = 0;

    HexapawnPresenter();

    oryx::SharedPtr<const oryx::BoardLayout> layout(const oryx::IState& state) const override;
    void describe_pieces(const oryx::IState& state, oryx::PlayerId viewer, oryx::BoardContent& out) const override;
    void action_picks(const oryx::IState& state, oryx::ActionId action, oryx::PickList& out) const override;
    oryx::PieceStyle piece_style(oryx::PieceKind kind, oryx::PlayerId owner) const override;

private:
    oryx::SharedPtr<const oryx::BoardLayout> m_layout;
};

} // namespace oasis
