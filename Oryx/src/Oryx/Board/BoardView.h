#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Game/PlayerId.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Math/Vector3.h"

namespace oryx
{

// An index into BoardView::spaces.
using SpaceId = uint32_t;
constexpr SpaceId kNoSpace = static_cast<SpaceId>(-1);

// What a piece is, numbered by the game (pawn, king, card...); IBoardPresenter::piece_style says how it looks.
using PieceKind = uint32_t;

enum class SpaceShape : uint8_t
{
    Square,
    Circle
};

// A place a piece can stand, in board units: +x right, +y away from the first player, +z up off the table (0 for flat boards).
// A front end chooses how board units reach the screen, so the same view serves the terminal, a 2D window and a 3D table.
struct BoardSpace
{
    Vec3f position;
    // Footprint on the table.
    Vec2f size{ 1.0f, 1.0f };
    SpaceShape shape = SpaceShape::Square;
    // Alternating shade (0 or 1) for checkered boards.
    uint8_t tone = 0;
    // What a person types to pick the space: unique within the view, no whitespace (e.g. "b3").
    std::string label;
};

struct BoardPiece
{
    PieceKind kind = 0;
    PlayerId owner = 0;
    SpaceId space = kNoSpace;
};

// Everything one seat sees of a state, as plain data; no front end ever reads the state itself.
struct BoardView
{
    std::vector<BoardSpace> spaces;
    std::vector<BoardPiece> pieces;
    // Optional axis names for a grid, left to right and bottom to top.
    std::vector<std::string> column_labels;
    std::vector<std::string> row_labels;
    std::string status;
};

enum class PieceShape : uint8_t
{
    Disc,
    Ring,
    Cross,
    Square
};

// How a piece looks to each front end: the terminal prints `glyph`, a 2D window draws `shape` in `colour`.
// A 3D front end will add a model here; games never name a rendering technique.
struct PieceStyle
{
    std::string glyph;
    Colour colour;
    PieceShape shape = PieceShape::Disc;
};

constexpr bool operator==(const BoardPiece& a, const BoardPiece& b)
{
    return a.kind == b.kind && a.owner == b.owner && a.space == b.space;
}

// Spaces laid out as a columns x rows grid with unit spacing, index row * columns + col where row 0 is the top row.
// Labels are chess-style ("a1" is the bottom-left space) and the axis labels are filled to match; `checkered` alternates tone.
void grid_spaces(uint32_t columns, uint32_t rows, bool checkered, BoardView& out);

// The space whose footprint contains `point` (board units, z ignored), or kNoSpace.
[[nodiscard]] SpaceId space_at(const BoardView& view, const Vec2f& point);

// The spaces whose pieces differ between two views of the same board, in ascending order; empty when the space layouts differ.
[[nodiscard]] std::vector<SpaceId> changed_spaces(const BoardView& before, const BoardView& after);

// Spaces with no labels, duplicate labels, labels containing whitespace or pieces on missing spaces throw Error naming `game`.
void validate_view(const BoardView& view, const std::string& game);

} // namespace oryx
