#include "BoardScene.h"

namespace oryx
{

namespace
{

constexpr float k_axis_tolerance = 1e-3f;

std::vector<float> distinct(std::vector<float> values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end(), [](float a, float b) { return std::abs(a - b) <= k_axis_tolerance; }), values.end());
    return values;
}

} // namespace

void build_board_scene(const BoardView& view, const IBoardPresenter& presenter, const MoveBuilder& builder, const SceneState& state, BoardScene& out)
{
    out.spaces.clear();
    out.pieces.clear();
    out.options.clear();
    out.column_labels = view.column_labels;
    out.row_labels = view.row_labels;
    out.status = view.status;
    out.min = { 0.0f, 0.0f };
    out.max = { 0.0f, 0.0f };

    for (size_t index = 0; index < view.spaces.size(); ++index)
    {
        const BoardSpace& space = view.spaces[index];
        out.spaces.push_back({ space, SpaceHighlight::None });

        Vec2f low = { space.position[0] - space.size[0] * 0.5f, space.position[1] - space.size[1] * 0.5f };
        Vec2f high = { space.position[0] + space.size[0] * 0.5f, space.position[1] + space.size[1] * 0.5f };
        out.min = index == 0 ? low : Vec2f(std::min(out.min[0], low[0]), std::min(out.min[1], low[1]));
        out.max = index == 0 ? high : Vec2f(std::max(out.max[0], high[0]), std::max(out.max[1], high[1]));
    }

    for (const BoardPiece& piece : view.pieces)
    {
        out.pieces.push_back({ presenter.piece_style(piece.kind, piece.owner), piece.space });
    }

    auto mark = [&](SpaceId space, SpaceHighlight bit)
    {
        if (space < out.spaces.size())
        {
            out.spaces[space].highlight |= bit;
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

    for (const Pick& pick : builder.next_picks())
    {
        if (pick.kind == PickKind::Option)
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

std::vector<float> scene_columns(const BoardScene& scene)
{
    std::vector<float> xs;
    for (const SceneSpace& space : scene.spaces)
    {
        xs.push_back(space.space.position[0]);
    }
    return distinct(std::move(xs));
}

std::vector<float> scene_rows(const BoardScene& scene)
{
    std::vector<float> ys;
    for (const SceneSpace& space : scene.spaces)
    {
        ys.push_back(space.space.position[1]);
    }
    ys = distinct(std::move(ys));
    std::reverse(ys.begin(), ys.end());
    return ys;
}

size_t axis_index(const std::vector<float>& axis, float value)
{
    for (size_t index = 0; index < axis.size(); ++index)
    {
        if (std::abs(axis[index] - value) <= k_axis_tolerance)
        {
            return index;
        }
    }
    return 0;
}

} // namespace oryx
