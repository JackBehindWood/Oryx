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

struct ScenePiece
{
    PieceStyle style;
    SpaceId space = k_no_space;
};

// A piece being dragged: its space and where the cursor is, in logical window points with the origin top left.
struct SceneDrag
{
    bool active = false;
    SpaceId space = k_no_space;
    Vec2f cursor;
};

// What a front end draws for one frame: the view resolved to styles and highlights, free of rendering types. The layout is shared, not copied.
struct BoardScene
{
    SharedPtr<const BoardLayout> layout;
    // One per layout space.
    std::vector<SpaceHighlight> highlights;
    std::vector<ScenePiece> pieces;
    // The option and Confirm picks to offer as a menu now; empty while the next pick is a space.
    PickList options;
    SceneDrag drag;
    std::string status;
    // Scratch reused between frames so building a scene allocates nothing once warm.
    PickList pending;
};

struct SceneState
{
    // Spaces to mark as changed by the last move.
    const std::vector<SpaceId>& changed;
    SpaceId hovered = k_no_space;
    SceneDrag drag;
};

void build_board_scene(const BoardView& view, const IBoardPresenter& presenter, const MoveBuilder& builder, const SceneState& state, BoardScene& out);

} // namespace oryx
