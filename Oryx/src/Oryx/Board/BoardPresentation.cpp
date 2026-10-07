#include "BoardPresentation.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

bool same_view(const BoardView& a, const BoardView& b)
{
    return a.layout == b.layout && a.pieces == b.pieces && a.status == b.status;
}

} // namespace

BoardPresentation::BoardPresentation(UniquePtr<IBoardPresenter> presenter, std::string game, PlayerId seat)
    : m_presenter(std::move(presenter))
    , m_game(std::move(game))
    , m_seat(seat)
{
    if (m_presenter == nullptr)
    {
        throw Error("BoardPresentation for '" + m_game + "' needs a presenter");
    }
}

void BoardPresentation::reset(PlayerId seat)
{
    m_seat = seat;
    m_view = {};
    m_next = {};
    m_changed.clear();
    m_legal.clear();
    m_builder.reset({});
    m_to_move = 0;
    m_winner = -1;
    m_terminal = false;
    m_described = false;
}

bool BoardPresentation::update(const IState& state)
{
    PlayerId to_move = state.current_player();
    bool terminal = state.is_terminal();
    ActionList legal = terminal ? ActionList{} : state.legal_actions();

    m_next.layout = m_presenter->layout(state);
    m_next.pieces.clear();
    m_next.status.clear();
    m_presenter->describe_pieces(state, m_seat == k_all_seats ? to_move : m_seat, m_next);

    if (m_described && same_view(m_view, m_next) && to_move == m_to_move && terminal == m_terminal && legal == m_legal)
    {
        return false;
    }

    validate_view(m_next, m_game);
    m_changed = m_described ? changed_spaces(m_view, m_next) : std::vector<SpaceId>{};
    std::swap(m_view, m_next);
    m_to_move = to_move;
    m_terminal = terminal;
    m_winner = terminal ? winner_of(state.outcome()) : -1;
    m_legal = std::move(legal);
    m_described = true;

    m_builder.reset(accepts_moves() ? collect_candidates(*m_presenter, state) : std::vector<MoveCandidate>{});
    return true;
}

bool BoardPresentation::accepts_moves() const
{
    return m_described && !m_terminal && (m_seat == k_all_seats || m_seat == m_to_move);
}

void BoardPresentation::build_scene(SpaceId hovered, BoardScene& out, const SceneDrag& drag) const
{
    build_board_scene(m_view, *m_presenter, m_builder, { m_changed, hovered, drag }, out);
}

} // namespace oryx
