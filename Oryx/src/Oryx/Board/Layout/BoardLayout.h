#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Math/Vector2.h"

namespace oryx
{

// An index into BoardLayout's spaces.
using SpaceId = uint32_t;
constexpr SpaceId k_no_space = static_cast<SpaceId>(-1);

enum class BoardDimension : uint8_t
{
    Planar,
    Spatial
};

// What a board is made of, fixed for the game and shared by every view of it: where each space sits on the table, how to name it and the derived
// rows and columns the terminal prints. Concrete derived layouts (BoardLayout2D) add the geometry their front end needs.
class BoardLayout
{
public:
    explicit BoardLayout(BoardDimension dimension)
        : m_dimension(dimension)
    {
    }
    virtual ~BoardLayout() = default;
    BoardLayout(const BoardLayout&) = delete;
    BoardLayout& operator=(const BoardLayout&) = delete;

    [[nodiscard]] BoardDimension dimension() const { return m_dimension; }
    [[nodiscard]] size_t space_count() const { return m_labels.size(); }
    // What a person types to pick the space: unique, no whitespace (e.g. "b3").
    [[nodiscard]] const std::string& label(SpaceId space) const { return m_labels[space]; }
    // Alternating shade (0 or 1) for checkered boards.
    [[nodiscard]] uint8_t tone(SpaceId space) const { return m_tones[space]; }
    // Position on the table: +x right, +y away from the first player.
    [[nodiscard]] const Vec2f& position(SpaceId space) const { return m_positions[space]; }

    // Optional axis names for a grid, left to right and bottom to top.
    [[nodiscard]] const std::vector<std::string>& column_labels() const { return m_column_labels; }
    [[nodiscard]] const std::vector<std::string>& row_labels() const { return m_row_labels; }
    // The distinct x positions ascending and the distinct y positions descending (top row first).
    [[nodiscard]] const std::vector<float>& columns() const { return m_columns; }
    [[nodiscard]] const std::vector<float>& rows() const { return m_rows; }
    [[nodiscard]] size_t column_index(SpaceId space) const { return m_column_of[space]; }
    [[nodiscard]] size_t row_index(SpaceId space) const { return m_row_of[space]; }
    // False when two spaces share a column and row, so the terminal lists spaces instead of printing a grid.
    [[nodiscard]] bool text_grid() const { return m_text_grid; }
    // The spaces' extent on the table, for fitting the board to a viewport.
    [[nodiscard]] const Vec2f& min() const { return m_min; }
    [[nodiscard]] const Vec2f& max() const { return m_max; }

protected:
    BoardLayout(BoardLayout&&) = default;

    void add_base_space(std::string label, uint8_t tone, const Vec2f& position, const Vec2f& half_extent);
    void set_axis_labels(std::vector<std::string> columns, std::vector<std::string> rows);
    // Validates the spaces and computes the axes and extent; throws Error on a bad label or geometry.
    void finalize_base();

private:
    BoardDimension m_dimension;
    std::vector<std::string> m_labels;
    std::vector<uint8_t> m_tones;
    std::vector<Vec2f> m_positions;
    std::vector<Vec2f> m_half_extents;
    std::vector<std::string> m_column_labels;
    std::vector<std::string> m_row_labels;
    std::vector<float> m_columns;
    std::vector<float> m_rows;
    std::vector<size_t> m_column_of;
    std::vector<size_t> m_row_of;
    bool m_text_grid = true;
    Vec2f m_min;
    Vec2f m_max;
};

// The layout as its concrete type; throws Error naming `game` when it is missing or another kind.
template <typename T>
[[nodiscard]] const T& layout_as(const SharedPtr<const BoardLayout>& layout, const std::string& game)
{
    if (layout == nullptr)
    {
        throw Error("Board of '" + game + "' has no layout");
    }
    if (layout->dimension() != T::k_dimension)
    {
        throw Error("Board of '" + game + "' has a layout this front end cannot draw");
    }
    return static_cast<const T&>(*layout);
}

} // namespace oryx
