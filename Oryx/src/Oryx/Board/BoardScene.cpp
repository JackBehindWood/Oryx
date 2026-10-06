#include "BoardScene.h"

namespace oryx
{

void build_board_scene(const BoardView& view, const IBoardPresenter& presenter, const MoveBuilder& builder, const SceneState& state, BoardScene& out)
{
    out.layout = view.layout;
    out.highlights.assign(view.layout != nullptr ? view.layout->space_count() : 0, SpaceHighlight::None);
    out.pieces.clear();
    out.options.clear();
    out.drag = state.drag;
    out.status = view.status;

    for (const BoardPiece& piece : view.pieces)
    {
        out.pieces.push_back({ presenter.piece_style(piece.kind, piece.owner), piece.space });
    }

    auto mark = [&](SpaceId space, SpaceHighlight bit)
    {
        if (space < out.highlights.size())
        {
            out.highlights[space] |= bit;
        }
    };

    for (SpaceId space : state.changed)
    {
        mark(space, SpaceHighlight::Changed);
    }

    for (const Pick& pick : builder.picked())
    {
        if (pick.kind == PickKind::Space)
        {
            mark(pick.value, SpaceHighlight::Picked);
        }
    }

    builder.next_picks(out.pending);
    for (const Pick& pick : out.pending)
    {
        if (pick.kind != PickKind::Space)
        {
            out.options.push_back(pick);
        }
        else if (!builder.picked().empty())
        {
            mark(pick.value, SpaceHighlight::Target);
        }
    }

    if (state.hovered != k_no_space && builder.can_pick({ PickKind::Space, state.hovered, {} }))
    {
        mark(state.hovered, SpaceHighlight::Hover);
    }
}

} // namespace oryx
