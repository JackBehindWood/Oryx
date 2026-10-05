#pragma once

#include "Oryx/Board/MoveBuilder.h"

namespace oryx
{

// Bits of SceneSpace::highlight.
enum SpaceHighlight : uint8_t
{
    kHighlightNone = 0,
    // Can be picked next while a move is being built.
    kHighlightTarget = 1 << 0,
    // Picked as part of the move being built.
    kHighlightPicked = 1 << 1,
    // Its pieces changed with the last move or undo.
    kHighlightChanged = 1 << 2,
    // Under the cursor and pickable.
    kHighlightHover = 1 << 3,
};

struct SceneSpace
{
    BoardSpace space;
    uint8_t highlight = kHighlightNone;
};

struct ScenePiece
{
    PieceStyle style;
    SpaceId space = kNoSpace;
};

// What a front end draws for one frame: the view resolved to styles and highlights, still in board units and free of any rendering type.
// It is the seam between presentation and rendering: the terminal prints it, BoardGraphics draws it in 2D, and a 3D renderer would draw it as models.
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
    SpaceId hovered = kNoSpace;
};

void build_board_scene(const BoardView& view, const IBoardPresenter& presenter, const MoveBuilder& builder, const SceneState& state, BoardScene& out);

// The distinct x positions of the spaces, ascending, and the distinct y positions, descending (top row first): the columns and rows of a grid.
[[nodiscard]] std::vector<float> scene_columns(const BoardScene& scene);
[[nodiscard]] std::vector<float> scene_rows(const BoardScene& scene);
// The index of `value` in an axis from scene_columns or scene_rows.
[[nodiscard]] size_t axis_index(const std::vector<float>& axis, float value);

} // namespace oryx
