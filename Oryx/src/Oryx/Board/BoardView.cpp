#include "BoardView.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

void pieces_by_space(const BoardView& view, std::vector<std::vector<BoardPiece>>& out)
{
    out.assign(view.layout->space_count(), {});
    for (const BoardPiece& piece : view.pieces)
    {
        if (piece.space < out.size())
        {
            out[piece.space].push_back(piece);
        }
    }
}

} // namespace

std::vector<SpaceId> changed_spaces(const BoardView& before, const BoardView& after)
{
    std::vector<SpaceId> changed;
    if (before.layout == nullptr || before.layout != after.layout)
    {
        return changed;
    }

    std::vector<std::vector<BoardPiece>> old_pieces;
    std::vector<std::vector<BoardPiece>> new_pieces;
    pieces_by_space(before, old_pieces);
    pieces_by_space(after, new_pieces);
    for (size_t index = 0; index < old_pieces.size(); ++index)
    {
        if (old_pieces[index] != new_pieces[index])
        {
            changed.push_back(static_cast<SpaceId>(index));
        }
    }
    return changed;
}

void validate_view(const BoardView& view, const std::string& game)
{
    if (view.layout == nullptr)
    {
        throw Error("Board view of '" + game + "' has no layout");
    }
    for (const BoardPiece& piece : view.pieces)
    {
        if (piece.space >= view.layout->space_count())
        {
            throw Error("Board view of '" + game + "' places a piece on missing space " + std::to_string(piece.space));
        }
    }
}

} // namespace oryx
