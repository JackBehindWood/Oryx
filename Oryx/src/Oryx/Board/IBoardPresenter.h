#pragma once

#include "Oryx/Board/BoardView.h"
#include "Oryx/Core/Registry.h"
#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

enum class PickKind : uint8_t
{
    // `value` is a SpaceId.
    Space,
    // `value` is a game-defined choice offered as a menu (e.g. the piece a pawn promotes to).
    Option
};

// One step of playing a move: a move is the sequence of picks a person makes, so the same presenter drives a mouse, a terminal or a controller.
struct Pick
{
    PickKind kind = PickKind::Space;
    uint32_t value = 0;
    // Menu text, for options only; spaces are named by their BoardSpace::label.
    std::string label;
};

constexpr bool operator==(const Pick& a, const Pick& b)
{
    return a.kind == b.kind && a.value == b.value;
}

using PickList = std::vector<Pick>;

// Plays a game through the generic front ends: describes states as BoardViews and moves as picks. Must be stateless; front ends call it in any order.
class IBoardPresenter
{
public:
    virtual ~IBoardPresenter() = default;

    // Fills `out`, which arrives cleared, with what `viewer` would see at the table; a seat must never be shown what it could not see.
    virtual void describe(const IState& state, PlayerId viewer, BoardView& out) const = 0;
    // The picks that play `action`, a legal action of `state`. Distinct actions need distinct sequences and none may be a prefix of another.
    virtual void action_picks(const IState& state, ActionId action, PickList& out) const = 0;
    [[nodiscard]] virtual PieceStyle piece_style(PieceKind kind, PlayerId owner) const = 0;
};

using BoardPresenterRegistry = Registry<IBoardPresenter>;

} // namespace oryx

// `game` is the registered game name the presenter shows.
#define OX_REGISTER_BOARD_PRESENTER(Type, game) \
    OX_REGISTER_FACTORY(::oryx::IBoardPresenter, Type, game)
