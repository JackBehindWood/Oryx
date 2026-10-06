#include "BoardLayout.h"

namespace oryx
{

namespace
{

constexpr float k_axis_tolerance = 1e-3f;

std::vector<float> distinct(std::vector<float> values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end(), [](float a, float b) { return std::abs(a - b) <= k_axis_tolerance; }), values.end());
    return values;
}

size_t axis_index(const std::vector<float>& axis, float value)
{
    for (size_t index = 0; index < axis.size(); ++index)
    {
        if (std::abs(axis[index] - value) <= k_axis_tolerance)
        {
            return index;
        }
    }
    return 0;
}

} // namespace

void BoardLayout::add_base_space(std::string label, uint8_t tone, const Vec2f& position, const Vec2f& half_extent)
{
    m_labels.push_back(std::move(label));
    m_tones.push_back(tone);
    m_positions.push_back(position);
    m_half_extents.push_back(half_extent);
}

void BoardLayout::set_axis_labels(std::vector<std::string> columns, std::vector<std::string> rows)
{
    m_column_labels = std::move(columns);
    m_row_labels = std::move(rows);
}

void BoardLayout::finalize_base()
{
    std::set<std::string> seen;
    for (size_t index = 0; index < m_labels.size(); ++index)
    {
        const std::string& label = m_labels[index];
        if (label.empty())
        {
            throw Error("Board layout has a space without a label");
        }
        if (std::any_of(label.begin(), label.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }))
        {
            throw Error("Board layout has a space label with whitespace: '" + label + "'");
        }
        if (!seen.insert(label).second)
        {
            throw Error("Board layout has two spaces labelled '" + label + "'");
        }
        const Vec2f& position = m_positions[index];
        const Vec2f& half = m_half_extents[index];
        if (!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(half[0]) || !std::isfinite(half[1]) || half[0] <= 0.0f || half[1] <= 0.0f)
        {
            throw Error("Board layout space '" + label + "' has a non-finite position or a non-positive size");
        }
    }

    std::vector<float> xs;
    std::vector<float> ys;
    m_min = { 0.0f, 0.0f };
    m_max = { 0.0f, 0.0f };
    for (size_t index = 0; index < m_positions.size(); ++index)
    {
        const Vec2f& position = m_positions[index];
        const Vec2f& half = m_half_extents[index];
        xs.push_back(position[0]);
        ys.push_back(position[1]);
        Vec2f low = { position[0] - half[0], position[1] - half[1] };
        Vec2f high = { position[0] + half[0], position[1] + half[1] };
        m_min = index == 0 ? low : Vec2f(std::min(m_min[0], low[0]), std::min(m_min[1], low[1]));
        m_max = index == 0 ? high : Vec2f(std::max(m_max[0], high[0]), std::max(m_max[1], high[1]));
    }

    m_columns = distinct(std::move(xs));
    m_rows = distinct(std::move(ys));
    std::reverse(m_rows.begin(), m_rows.end());

    m_column_of.clear();
    m_row_of.clear();
    std::set<std::pair<size_t, size_t>> cells;
    m_text_grid = true;
    for (const Vec2f& position : m_positions)
    {
        size_t column = axis_index(m_columns, position[0]);
        size_t row = axis_index(m_rows, position[1]);
        m_column_of.push_back(column);
        m_row_of.push_back(row);
        m_text_grid = cells.insert({ column, row }).second && m_text_grid;
    }
}

} // namespace oryx
