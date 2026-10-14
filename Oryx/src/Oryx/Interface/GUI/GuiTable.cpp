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

class TableFrame
{
public:
    TableFrame(std::string_view name, const TableOptions& options);
    ~TableFrame();

    TableFrame(const TableFrame&) = delete;
    TableFrame& operator=(const TableFrame&) = delete;

    void column(std::string_view label, Sizing width, bool sortable, TextAlign align, bool tabular);
    TableResult headers();
    [[nodiscard]] uint32_t first_row() const { return m_first; }
    [[nodiscard]] uint32_t last_row() const { return m_last; }
    ItemState row(uint32_t index, bool selected);
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
        bool tabular = false;
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

TableFrame& table_frame(const char* user)
{
    return context().scope<TableFrame>(user);
}

TableFrame::TableFrame(std::string_view name, const TableOptions& options)
    : m_context(context())
    , m_style(&m_context.role_style(options, &GuiTheme::panel))
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

TableFrame::~TableFrame()
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

void TableFrame::column(std::string_view label, Sizing width, bool sortable, TextAlign align, bool tabular)
{
    if (m_headers)
    {
        throw Error("TableScope column added after headers()", "declare every column before headers()");
    }
    if (m_column_count >= k_max_table_columns)
    {
        throw Error("TableScope holds at most sixteen columns");
    }
    m_columns[m_column_count++] = { m_context.arena().store(label), width, align, sortable, tabular };
}

void TableFrame::spacer_rows(uint32_t count)
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

TableResult TableFrame::headers()
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
        strip.height = fixed(theme.bar_height);
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

void TableFrame::close_row()
{
    if (m_row_open)
    {
        m_context.pop_id();
        m_context.end_box();
        m_row_open = false;
    }
}

ItemState TableFrame::row(uint32_t index, bool selected)
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
    paint.fill = selected ? im::keep_fill(state, style.selected) : state.held ? style.pressed : style.hover;
    ctx.push_id(id);
    m_row_open = true;
    m_cell = 0;
    return state;
}

void TableFrame::begin_cell()
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

void TableFrame::end_cell()
{
    m_context.end_box();
    m_cell_open = false;
}

void TableFrame::cell(std::string_view text)
{
    begin_cell();
    BoxPaint& paint = m_context.layout().node(m_context.layout().node_count() - 1).paint;
    const Column& column = m_columns[m_cell - 1];
    paint.text = m_context.arena().store(text);
    paint.text_height = m_style->text_height;
    paint.text_colour = m_style->text;
    paint.text_align = column.align;
    paint.ellipsis = true;
    paint.tabular = column.tabular;
    end_cell();
}

} // namespace

bool begin_table(std::string_view name, const TableOptions& options)
{
    context().push_scope<TableFrame>("table", name, options);
    return true;
}

void end_table()
{
    context().pop_scope<TableFrame>("end_table");
}

void table_column(std::string_view label, Sizing width, bool sortable, TextAlign align, bool tabular)
{
    table_frame("table_column").column(label, width, sortable, align, tabular);
}

TableResult table_headers()
{
    return table_frame("table_headers").headers();
}

uint32_t table_first_row()
{
    return table_frame("table_first_row").first_row();
}

uint32_t table_last_row()
{
    return table_frame("table_last_row").last_row();
}

ItemState table_row(uint32_t index, bool selected)
{
    return table_frame("table_row").row(index, selected);
}

void table_cell(std::string_view text)
{
    table_frame("table_cell").cell(text);
}

void table_begin_cell()
{
    table_frame("table_begin_cell").begin_cell();
}

void table_end_cell()
{
    table_frame("table_end_cell").end_cell();
}

} // namespace oryx::gui
