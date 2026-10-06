#pragma once

#include "Oryx/Board/BoardScene.h"

namespace oryx
{

// The seat a board plays for in hot-seat games: every seat, each shown from its own side on its turn.
constexpr PlayerId k_all_seats = -1;

// What every presented front end shares, kept apart from how it draws or reads input: the current view, what the last move changed,
// and the move being built. Front ends own one and feed it each turn.
class BoardPresentation
{
public:
    // `seat` is the local human's player, or k_all_seats; it decides whose view is shown and when moves are accepted.
    BoardPresentation(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat);

    // Describes the state again. True when anything shown or playable changed, which also drops a half-built move.
    // Throws Error if the presenter produces an invalid view or ambiguous picks.
    bool update(const IState& state);

    // A local human may move now.
    [[nodiscard]] bool accepts_moves() const;
    [[nodiscard]] bool terminal() const { return m_terminal; }
    [[nodiscard]] const BoardView& view() const { return m_view; }
    [[nodiscard]] const std::vector<SpaceId>& changed() const { return m_changed; }
    [[nodiscard]] const IBoardPresenter& presenter() const { return *m_presenter; }
    [[nodiscard]] MoveBuilder& builder() { return m_builder; }
    [[nodiscard]] const MoveBuilder& builder() const { return m_builder; }

    void build_scene(SpaceId hovered, BoardScene& out, const SceneDrag& drag = {}) const;

private:
    UniquePtr<IBoardPresenter> m_presenter;
    std::string m_game;
    PlayerId m_seat;
    BoardView m_view;
    BoardView m_next;
    std::vector<SpaceId> m_changed;
    MoveBuilder m_builder;
    ActionList m_legal;
    PlayerId m_to_move = 0;
    bool m_terminal = false;
    bool m_described = false;
};

} // namespace oryx
