#pragma once

#include "Oryx/Board/Layout/BoardLayout.h"
#include "Oryx/Core/Base.h"
#include "Oryx/Game/PlayerId.h"
#include "Oryx/Math/Colour.h"

namespace oryx
{

// What a piece is, numbered by the game (pawn, king, card...); IBoardPresenter::piece_style says how it looks.
using PieceKind = uint32_t;

struct BoardPiece
{
    PieceKind kind = 0;
    PlayerId owner = 0;
    SpaceId space = k_no_space;
};

// What one seat sees of a state apart from the board itself.
struct BoardContent
{
    std::vector<BoardPiece> pieces;
    std::string status;
};

// Everything one seat sees of a state, as plain data; no front end ever reads the state itself.
struct BoardView : BoardContent
{
    SharedPtr<const BoardLayout> layout;
};

enum class PieceShape : uint8_t
{
    Disc,
    Ring,
    Cross,
    Square
};

// How a piece looks to each front end: the terminal prints `glyph`, a 2D window draws `shape` in `colour`.
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

// The spaces whose pieces differ between two views of the same board, in ascending order; empty when the layouts differ.
[[nodiscard]] std::vector<SpaceId> changed_spaces(const BoardView& before, const BoardView& after);

// A missing layout or pieces on missing spaces throw Error naming `game`.
void validate_view(const BoardView& view, const std::string& game);

} // namespace oryx
