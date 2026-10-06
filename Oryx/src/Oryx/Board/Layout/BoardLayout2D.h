#pragma once

#include "Oryx/Board/Layout/BoardLayout.h"

namespace oryx
{

enum class SpaceShape : uint8_t
{
    Square,
    Circle
};

// A flat board: every space has a footprint and shape on the table. Build with add_space, then finish() once to validate and share it.
class BoardLayout2D final : public BoardLayout
{
public:
    static constexpr BoardDimension k_dimension = BoardDimension::Planar;

    BoardLayout2D()
        : BoardLayout(k_dimension)
    {
    }

    SpaceId add_space(std::string label, const Vec2f& position, const Vec2f& size, SpaceShape shape, uint8_t tone = 0);
    void set_axis_labels(std::vector<std::string> columns, std::vector<std::string> rows) { BoardLayout::set_axis_labels(std::move(columns), std::move(rows)); }
    // Throws Error on an empty, duplicate or whitespace label or bad geometry; the builder is spent afterwards.
    [[nodiscard]] SharedPtr<const BoardLayout2D> finish();

    [[nodiscard]] const Vec2f& size(SpaceId space) const { return m_sizes[space]; }
    [[nodiscard]] SpaceShape shape(SpaceId space) const { return m_shapes[space]; }

private:
    BoardLayout2D(BoardLayout2D&&) = default;

    std::vector<Vec2f> m_sizes;
    std::vector<SpaceShape> m_shapes;
};

// A columns x rows grid of unit squares, index row * columns + col where row 0 is the top row; labels are chess-style ("a1" is the bottom-left
// space) and `checkered` alternates tone.
[[nodiscard]] SharedPtr<const BoardLayout2D> make_grid_layout(uint32_t columns, uint32_t rows, bool checkered);
// The same layout for the same arguments, so a presenter can pick a size from the state and still return a stable pointer.
[[nodiscard]] SharedPtr<const BoardLayout2D> shared_grid_layout(uint32_t columns, uint32_t rows, bool checkered);

// The space whose footprint contains `point` (board units), or k_no_space.
[[nodiscard]] SpaceId space_at(const BoardLayout2D& layout, const Vec2f& point);

} // namespace oryx
