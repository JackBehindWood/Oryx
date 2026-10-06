#pragma once

#include "Oryx/Board/MoveBuilder.h"

namespace oryx
{

// Bit flags of SceneSpace::highlight.
enum class SpaceHighlight : uint8_t
{
    None = 0,
    // Can be picked next while a move is being built.
    Target = 1 << 0,
    // Picked as part of the move being built.
    Picked = 1 << 1,
    // Its pieces changed with the last move or undo.
    Changed = 1 << 2,
    // Under the cursor and pickable.
    Hover = 1 << 3,
};

constexpr SpaceHighlight operator|(SpaceHighlight a, SpaceHighlight b)
{
    return static_cast<SpaceHighlight>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr SpaceHighlight& operator|=(SpaceHighlight& a, SpaceHighlight b)
{
    a = a | b;
    return a;
}

constexpr bool has_highlight(SpaceHighlight set, SpaceHighlight flag)
{
    return (static_cast<uint8_t>(set) & static_cast<uint8_t>(flag)) != 0;
}

struct SceneSpace
{
    BoardSpace space;
    SpaceHighlight highlight = SpaceHighlight::None;
};

struct ScenePiece
{
    PieceStyle style;
    SpaceId space = k_no_space;
};

// What a front end draws for one frame: the view resolved to styles and highlights, in board units and free of rendering types.
struct BoardScene
{
    std::vector<SceneSpace> spaces;
    std::vector<ScenePiece> pieces;
    std::vector<std::string> column_labels;
    std::vector<std::string> row_labels;
    // The option picks to offer as a menu now; empty while the next pick is a space.
    PickList options;
    std::string status;
    // The spaces' extent on the table (x, y), for fitting the board to a viewport.
    Vec2f min;
    Vec2f max;
};

struct SceneState
{
    // Spaces to mark as changed by the last move.
    std::vector<SpaceId> changed;
    SpaceId hovered = k_no_space;
};

void build_board_scene(const BoardView& view, const IBoardPresenter& presenter, const MoveBuilder& builder, const SceneState& state, BoardScene& out);

// The distinct x positions of the spaces, ascending, and the distinct y positions, descending (top row first): the columns and rows of a grid.
[[nodiscard]] std::vector<float> scene_columns(const BoardScene& scene);
[[nodiscard]] std::vector<float> scene_rows(const BoardScene& scene);
// The index of `value` in an axis from scene_columns or scene_rows.
[[nodiscard]] size_t axis_index(const std::vector<float>& axis, float value);

} // namespace oryx
