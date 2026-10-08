#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// A table in the manner of ImGui's: declare the columns, draw the header, then submit only the rows that are visible. The caller owns the data and its order, and sorts it when the header says so.
namespace oryx::gui
{

inline constexpr uint32_t k_max_table_columns = 16;

struct TableOptions : WidgetOptions
{
    // The rows in the whole table, so the scroll bar spans them all though only the visible ones are submitted.
    uint32_t row_count = 0;
    // Every row is this tall, which is what lets the table skip the rows outside the view.
    float row_height = 22.0f;
    Sizing width = grow();
    Sizing height = grow();
};

static_assert(std::is_trivially_copyable_v<TableOptions>);

struct TableResult
{
    // The column the caller should sort by, k_no_index until a sortable header was clicked.
    uint32_t sort_column = k_no_index;
    bool sort_ascending = true;
    // A header was clicked this frame.
    bool sort_changed = false;
};

static_assert(std::is_trivially_copyable_v<TableResult> && std::is_standard_layout_v<TableResult>);

// Use: column() for each column, headers(), then `for row in first_row()..last_row()` row() followed by one cell() (or begin_cell()/end_cell() around widgets of your own) per column.
// first_row/last_row come from last frame's scroll offset and view height, with a row of margin each side, so a table is blank-free but one frame late like every hit area. Throws Error for more than k_max_table_columns,
// for more cells than columns, and for rows or cells before headers().
class TableScope
{
public:
    TableScope(std::string_view name, const TableOptions& options);
    ~TableScope();

    TableScope(const TableScope&) = delete;
    TableScope& operator=(const TableScope&) = delete;

    void column(std::string_view label, Sizing width = grow(), bool sortable = false, TextAlign align = TextAlign::Left);
    // Draws the header row and opens the scrolling body; sortable headers toggle their direction on a click.
    TableResult headers();

    [[nodiscard]] uint32_t first_row() const { return m_first; }
    [[nodiscard]] uint32_t last_row() const { return m_last; }

    // Starts a row (closing the previous one); the state is the pointer's relation to the row, whose click is the caller's selection.
    ItemState row(uint32_t index, bool selected = false);
    void cell(std::string_view text);
    void begin_cell();
    void end_cell();

private:
    struct Column
    {
        std::string_view label;
        Sizing width;
        TextAlign align = TextAlign::Left;
        bool sortable = false;
    };

    void close_row();
    void spacer_rows(uint32_t count);

    GuiContext& m_context;
    const ImStyle* m_style;
    TableOptions m_options;
    ImId m_id;
    Column m_columns[k_max_table_columns];
    uint32_t m_column_count = 0;
    uint32_t m_first = 0;
    uint32_t m_last = 0;
    uint32_t m_cell = 0;
    bool m_headers = false;
    bool m_row_open = false;
    bool m_cell_open = false;
};

} // namespace oryx::gui
