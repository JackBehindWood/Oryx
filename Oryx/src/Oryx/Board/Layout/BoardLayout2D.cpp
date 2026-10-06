#include "BoardLayout2D.h"

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

bool contains(const BoardLayout2D& layout, SpaceId space, const Vec2f& point)
{
    const Vec2f& position = layout.position(space);
    float dx = point[0] - position[0];
    float dy = point[1] - position[1];
    float half_x = layout.size(space)[0] * 0.5f;
    float half_y = layout.size(space)[1] * 0.5f;
    if (layout.shape(space) == SpaceShape::Circle)
    {
        float radius = std::min(half_x, half_y);
        return dx * dx + dy * dy <= radius * radius;
    }
    return std::abs(dx) <= half_x && std::abs(dy) <= half_y;
}

} // namespace

SpaceId BoardLayout2D::add_space(std::string label, const Vec2f& position, const Vec2f& size, SpaceShape shape, uint8_t tone)
{
    SpaceId id = static_cast<SpaceId>(space_count());
    add_base_space(std::move(label), tone, position, { size[0] * 0.5f, size[1] * 0.5f });
    m_sizes.push_back(size);
    m_shapes.push_back(shape);
    return id;
}

SharedPtr<const BoardLayout2D> BoardLayout2D::finish()
{
    finalize_base();
    return SharedPtr<const BoardLayout2D>(new BoardLayout2D(std::move(*this)));
}

SharedPtr<const BoardLayout2D> make_grid_layout(uint32_t columns, uint32_t rows, bool checkered)
{
    BoardLayout2D layout;
    for (uint32_t row = 0; row < rows; ++row)
    {
        uint32_t rank = rows - 1 - row;
        for (uint32_t col = 0; col < columns; ++col)
        {
            uint8_t tone = checkered ? static_cast<uint8_t>((col + rank + 1) % 2) : 0;
            layout.add_space(column_name(col) + std::to_string(rank + 1), { static_cast<float>(col), static_cast<float>(rank) }, { 1.0f, 1.0f }, SpaceShape::Square, tone);
        }
    }

    std::vector<std::string> column_labels;
    for (uint32_t col = 0; col < columns; ++col)
    {
        column_labels.push_back(column_name(col));
    }
    std::vector<std::string> row_labels;
    for (uint32_t rank = 0; rank < rows; ++rank)
    {
        row_labels.push_back(std::to_string(rank + 1));
    }
    layout.set_axis_labels(std::move(column_labels), std::move(row_labels));
    return layout.finish();
}

SharedPtr<const BoardLayout2D> shared_grid_layout(uint32_t columns, uint32_t rows, bool checkered)
{
    static std::mutex mutex;
    static std::map<std::array<uint32_t, 3>, SharedPtr<const BoardLayout2D>> cache;

    std::lock_guard<std::mutex> lock(mutex);
    SharedPtr<const BoardLayout2D>& slot = cache[{ columns, rows, checkered ? 1u : 0u }];
    if (slot == nullptr)
    {
        slot = make_grid_layout(columns, rows, checkered);
    }
    return slot;
}

SpaceId space_at(const BoardLayout2D& layout, const Vec2f& point)
{
    for (SpaceId space = 0; space < layout.space_count(); ++space)
    {
        if (contains(layout, space, point))
        {
            return space;
        }
    }
    return k_no_space;
}

} // namespace oryx
