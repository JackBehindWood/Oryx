#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"
#include "Oryx/Interface/GUI/GuiScope.h"

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

// Use: begin_table, table_column for each column, table_headers(), then `for row in table_first_row()..table_last_row()` table_row() followed by one table_cell() (or table_begin_cell()/table_end_cell() around widgets of your own) per column, then end_table().
// first_row/last_row come from last frame's scroll offset and view height, with a row of margin each side, so a table is blank-free but one frame late like every hit area. Throws Error for more than k_max_table_columns,
// for more cells than columns, and for rows or cells before headers(). begin_table always opens, so end_table always follows; TableScope does both.
bool begin_table(std::string_view name, const TableOptions& options);
void end_table();
void table_column(std::string_view label, Sizing width = grow(), bool sortable = false, TextAlign align = TextAlign::Left);
// Draws the header row and opens the scrolling body; sortable headers toggle their direction on a click.
TableResult table_headers();
[[nodiscard]] uint32_t table_first_row();
[[nodiscard]] uint32_t table_last_row();
// Starts a row (closing the previous one); the state is the pointer's relation to the row, whose click is the caller's selection.
ItemState table_row(uint32_t index, bool selected = false);
void table_cell(std::string_view text);
void table_begin_cell();
void table_end_cell();

struct TableEnd
{
    static void end() { end_table(); }
};

class TableScope : public Scope<TableEnd>
{
public:
    TableScope(std::string_view name, const TableOptions& options)
        : Scope(begin_table(name, options))
    {
    }

    void column(std::string_view label, Sizing width = grow(), bool sortable = false, TextAlign align = TextAlign::Left) { table_column(label, width, sortable, align); }
    TableResult headers() { return table_headers(); }
    [[nodiscard]] uint32_t first_row() const { return table_first_row(); }
    [[nodiscard]] uint32_t last_row() const { return table_last_row(); }
    ItemState row(uint32_t index, bool selected = false) { return table_row(index, selected); }
    void cell(std::string_view text) { table_cell(text); }
    void begin_cell() { table_begin_cell(); }
    void end_cell() { table_end_cell(); }
};

} // namespace oryx::gui
