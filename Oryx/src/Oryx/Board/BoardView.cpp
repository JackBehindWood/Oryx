#include "BoardView.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

// "a".."z", then "aa", "ab"... as spreadsheet columns.
std::string column_name(uint32_t index)
{
    std::string name;
    for (uint32_t value = index + 1; value > 0; value = (value - 1) / 26)
    {
        name.insert(name.begin(), static_cast<char>('a' + (value - 1) % 26));
    }
    return name;
}

bool contains(const BoardSpace& space, const Vec2f& point)
{
    float dx = point[0] - space.position[0];
    float dy = point[1] - space.position[1];
    float half_x = space.size[0] * 0.5f;
    float half_y = space.size[1] * 0.5f;
    if (space.shape == SpaceShape::Circle)
    {
        float radius = std::min(half_x, half_y);
        return dx * dx + dy * dy <= radius * radius;
    }
    return std::abs(dx) <= half_x && std::abs(dy) <= half_y;
}

void pieces_by_space(const BoardView& view, std::vector<std::vector<BoardPiece>>& out)
{
    out.assign(view.spaces.size(), {});
    for (const BoardPiece& piece : view.pieces)
    {
        if (piece.space < out.size())
        {
            out[piece.space].push_back(piece);
        }
    }
}

} // namespace

void grid_spaces(uint32_t columns, uint32_t rows, bool checkered, BoardView& out)
{
    out.spaces.clear();
    out.spaces.reserve(static_cast<size_t>(columns) * rows);
    for (uint32_t row = 0; row < rows; ++row)
    {
        uint32_t rank = rows - 1 - row;
        for (uint32_t col = 0; col < columns; ++col)
        {
            BoardSpace space;
            space.position = { static_cast<float>(col), static_cast<float>(rank), 0.0f };
            space.tone = checkered ? static_cast<uint8_t>((col + rank + 1) % 2) : 0;
            space.label = column_name(col) + std::to_string(rank + 1);
            out.spaces.push_back(std::move(space));
        }
    }

    out.column_labels.clear();
    for (uint32_t col = 0; col < columns; ++col)
    {
        out.column_labels.push_back(column_name(col));
    }
    out.row_labels.clear();
    for (uint32_t rank = 0; rank < rows; ++rank)
    {
        out.row_labels.push_back(std::to_string(rank + 1));
    }
}

SpaceId space_at(const BoardView& view, const Vec2f& point)
{
    for (size_t index = 0; index < view.spaces.size(); ++index)
    {
        if (contains(view.spaces[index], point))
        {
            return static_cast<SpaceId>(index);
        }
    }
    return k_no_space;
}

std::vector<SpaceId> changed_spaces(const BoardView& before, const BoardView& after)
{
    std::vector<SpaceId> changed;
    if (before.spaces.size() != after.spaces.size())
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
    std::set<std::string> labels;
    for (const BoardSpace& space : view.spaces)
    {
        if (space.label.empty())
        {
            throw Error("Board view of '" + game + "' has a space without a label");
        }
        if (std::any_of(space.label.begin(), space.label.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }))
        {
            throw Error("Board view of '" + game + "' has a space label with whitespace: '" + space.label + "'");
        }
        if (!labels.insert(space.label).second)
        {
            throw Error("Board view of '" + game + "' has two spaces labelled '" + space.label + "'");
        }
    }
    for (const BoardPiece& piece : view.pieces)
    {
        if (piece.space >= view.spaces.size())
        {
            throw Error("Board view of '" + game + "' places a piece on missing space " + std::to_string(piece.space));
        }
    }
}

} // namespace oryx
