#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiTable.h"

namespace oryx::gui
{

namespace
{

struct SortState
{
    uint32_t column = k_no_index;
    bool ascending = true;
};

constexpr uint32_t k_unknown_view_rows = 24;
constexpr uint32_t k_overscan_rows = 1;
constexpr const char* k_body = "body";

LayoutStyle cell_box(const Sizing& width, TextAlign align)
{
    LayoutStyle box;
    box.width = width;
    box.height = grow();
    box.padding = { 4.0f, 0.0f, 4.0f, 0.0f };
    box.align_y = Align::Centre;
    box.align_x = align == TextAlign::Left ? Align::Start : align == TextAlign::Centre ? Align::Centre : Align::End;
    return box;
}

} // namespace

TableScope::TableScope(std::string_view name, const TableOptions& options)
    : m_context(context())
    , m_style(&im::resolved_style(m_context, options))
    , m_options(options)
    , m_id(m_context.id(name))
{
    LayoutStyle box;
    box.width = options.width;
    box.height = options.height;
    box.direction = Direction::Column;
    m_context.begin_box(name, box);
    m_context.push_id(name);
}

TableScope::~TableScope()
{
    GuiContext& ctx = m_context;
    if (m_headers)
    {
        close_row();
        spacer_rows(m_options.row_count - m_last);
        end_scroll();
    }
    ctx.pop_id();
    ctx.end_box();
}

void TableScope::column(std::string_view label, Sizing width, bool sortable, TextAlign align)
{
    if (m_headers)
    {
        throw Error("TableScope column added after headers()", "declare every column before headers()");
    }
    if (m_column_count >= k_max_table_columns)
    {
        throw Error("TableScope holds at most sixteen columns");
    }
    m_columns[m_column_count++] = { m_context.arena().store(label), width, align, sortable };
}

void TableScope::spacer_rows(uint32_t count)
{
    if (count == 0)
    {
        return;
    }
    LayoutStyle box;
    box.width = grow();
    box.height = fixed(static_cast<float>(count) * m_options.row_height);
    m_context.begin_box(ImId{}, box);
    m_context.end_box();
}

TableResult TableScope::headers()
{
    if (m_headers)
    {
        throw Error("TableScope headers() called twice");
    }
    GuiContext& ctx = m_context;
    const ImStyle& style = *m_style;
    const GuiTheme& theme = ctx.gui_theme();

    const ImId body_id = ctx.id(k_body);
    Rect view;
    Vec2f content;
    const bool laid_out = ctx.layout_rect(body_id, view) && ctx.layout().content_size_of(body_id, content);
    const float view_height = laid_out ? view.size[1] : m_options.row_height * static_cast<float>(k_unknown_view_rows);
    const bool scrolls = laid_out && content[1] > view.size[1];
    const float offset = scroll_offset(k_body);
    const uint32_t rows = m_options.row_count;
    const float row_height = math::max(m_options.row_height, 1.0f);
    const uint32_t first = static_cast<uint32_t>(math::max(offset / row_height, 0.0f));
    const uint32_t last = static_cast<uint32_t>(math::max((offset + view_height) / row_height, 0.0f)) + 1;
    m_first = math::min(first > k_overscan_rows ? first - k_overscan_rows : 0u, rows);
    m_last = math::min(last + k_overscan_rows, rows);

    SortState sort = ctx.state<SortState>(m_id);
    TableResult result;
    {
        LayoutStyle strip;
        strip.width = grow();
        strip.height = fixed(row_height);
        strip.padding.right = scrolls ? theme.scrollbar_width : 0.0f;
        strip.align_y = Align::Centre;
        const uint32_t strip_index = ctx.begin_box(ctx.id("header"), strip);
        ctx.layout().node(strip_index).paint.has_fill = true;
        ctx.layout().node(strip_index).paint.fill = style.background;
        IdScope scope(ctx, "header");
        for (uint32_t index = 0; index < m_column_count; ++index)
        {
            const Column& column = m_columns[index];
            ItemState state;
            IdScope column_scope(ctx, ctx.index_id(index));
            const ImId cell_id = ctx.id("cell");
            if (column.sortable)
            {
                state = ctx.item(cell_id);
                if (state.clicked)
                {
                    sort.ascending = sort.column == index ? !sort.ascending : true;
                    sort.column = index;
                    result.sort_changed = true;
                }
            }
            const uint32_t cell_index = ctx.begin_box(cell_id, cell_box(column.width, column.align));
            BoxPaint& paint = ctx.layout().node(cell_index).paint;
            paint.text = sort.column == index ? ctx.arena().format("%.*s %s", static_cast<int32_t>(column.label.size()), column.label.data(), sort.ascending ? "^" : "v") : column.label;
            paint.text_height = style.text_height;
            paint.text_colour = style.text;
            paint.text_align = column.align;
            paint.ellipsis = true;
            paint.has_fill = state.hovered;
            paint.fill = style.hover;
            ctx.end_box();
        }
        ctx.end_box();
    }
    result.sort_column = sort.column;
    result.sort_ascending = sort.ascending;
    ctx.state<SortState>(m_id) = sort;

    ScrollOptions body;
    body.width = grow();
    body.height = grow();
    begin_scroll(k_body, body);
    m_headers = true;
    spacer_rows(m_first);
    return result;
}

void TableScope::close_row()
{
    if (m_row_open)
    {
        m_context.pop_id();
        m_context.end_box();
        m_row_open = false;
    }
}

ItemState TableScope::row(uint32_t index, bool selected)
{
    if (!m_headers)
    {
        throw Error("TableScope row before headers()", "call headers() once the columns are declared");
    }
    GuiContext& ctx = m_context;
    close_row();
    const ImStyle& style = *m_style;
    const ImId id = ctx.index_id(index);
    const ItemState state = ctx.item(id);
    LayoutStyle box;
    box.width = grow();
    box.height = fixed(m_options.row_height);
    box.align_y = Align::Centre;
    const uint32_t node = ctx.begin_box(id, box);
    BoxPaint& paint = ctx.layout().node(node).paint;
    paint.has_fill = selected || state.hovered;
    paint.fill = state.held ? style.pressed : selected ? style.accent : style.hover;
    ctx.push_id(id);
    m_row_open = true;
    m_cell = 0;
    return state;
}

void TableScope::begin_cell()
{
    if (!m_row_open)
    {
        throw Error("TableScope cell without a row", "call row() first");
    }
    if (m_cell >= m_column_count)
    {
        throw Error("TableScope row has more cells than columns");
    }
    const Column& column = m_columns[m_cell++];
    m_context.begin_box(ImId{}, cell_box(column.width, column.align));
    m_cell_open = true;
}

void TableScope::end_cell()
{
    m_context.end_box();
    m_cell_open = false;
}

void TableScope::cell(std::string_view text)
{
    begin_cell();
    BoxPaint& paint = m_context.layout().node(m_context.layout().node_count() - 1).paint;
    const Column& column = m_columns[m_cell - 1];
    paint.text = m_context.arena().store(text);
    paint.text_height = m_style->text_height;
    paint.text_colour = m_style->text;
    paint.text_align = column.align;
    paint.ellipsis = true;
    end_cell();
}

} // namespace oryx::gui
