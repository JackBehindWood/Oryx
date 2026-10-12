#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"
#include "Oryx/Interface/GUI/GuiTable.h"

// Optional: range-for over the rows a virtualised table or list box asks for, so the first/last bookkeeping disappears. Nothing in the GUI includes this; the primitives stay complete without it.
//   if (TableScope table("t", options); table) { table.column(...); table.headers(); for (uint32_t row : gui::table_rows()) { table.row(row); table.cell(...); } }
namespace oryx::gui
{

// The indices [first, last) in order.
class IndexRange
{
public:
    class Iterator
    {
    public:
        explicit Iterator(uint32_t at)
            : m_at(at)
        {
        }
        [[nodiscard]] uint32_t operator*() const { return m_at; }
        Iterator& operator++()
        {
            ++m_at;
            return *this;
        }
        [[nodiscard]] bool operator!=(const Iterator& other) const { return m_at != other.m_at; }

    private:
        uint32_t m_at;
    };

    IndexRange(uint32_t first, uint32_t last)
        : m_first(first)
        , m_last(last > first ? last : first)
    {
    }

    [[nodiscard]] Iterator begin() const { return Iterator(m_first); }
    [[nodiscard]] Iterator end() const { return Iterator(m_last); }
    [[nodiscard]] uint32_t size() const { return m_last - m_first; }

private:
    uint32_t m_first;
    uint32_t m_last;
};

// The rows of the innermost open table / list box that are in view (see table_first_row, list_first_item); call after table_headers() for a table.
[[nodiscard]] inline IndexRange table_rows()
{
    return IndexRange(table_first_row(), table_last_row());
}

[[nodiscard]] inline IndexRange list_items()
{
    return IndexRange(list_first_item(), list_last_item());
}

} // namespace oryx::gui
