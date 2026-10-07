#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

namespace
{

struct OpenState
{
    bool open = false;
    bool initialised = false;
};

struct ScrollState
{
    float offset = 0.0f;
    bool hovered = false;
};

constexpr float k_min_thumb = 16.0f;
constexpr uint32_t k_unknown_list_rows = 8;

bool toggled_open(GuiContext& ctx, ImId id, bool default_open, bool toggle)
{
    OpenState& state = ctx.state<OpenState>(id);
    if (!state.initialised)
    {
        state.initialised = true;
        state.open = default_open;
    }
    state.open = toggle ? !state.open : state.open;
    return state.open;
}

char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr float k_icon_box = 16.0f;

// A fixed square box that draws `icon`; the caller adds a hover fill to it and closes it.
uint32_t begin_icon_box(GuiContext& ctx, ImId id, Icon icon, const Colour& colour, float side = k_icon_box)
{
    LayoutStyle box;
    box.width = fixed(side);
    box.height = fixed(side);
    const uint32_t index = ctx.begin_box(id, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.icon = icon;
    paint.icon_colour = colour;
    return index;
}

Colour muted(const Colour& colour, float alpha = 0.75f)
{
    Colour result = colour;
    result.a *= alpha;
    return result;
}

// A text-only box that takes the rest of its row.
void title_box(GuiContext& ctx, std::string_view label, const ImStyle& style)
{
    LayoutStyle box;
    box.width = grow();
    box.align_x = Align::Start;
    const uint32_t index = ctx.begin_box(ImId{}, box);
    LayoutNode& node = ctx.layout().node(index);
    im::paint_text(node.paint, node, style, TextAlign::Left, true);
    node.paint.text = ctx.arena().store(label);
    ctx.end_box();
}

// Scroll range from the latest solve; false before the box has been laid out.
bool scroll_range(GuiContext& ctx, ImId id, Rect& view, Vec2f& content)
{
    return ctx.layout_rect(id, view) && ctx.layout().content_size_of(id, content);
}

} // namespace

namespace
{

bool header_impl(std::string_view label, bool* visible, bool default_open, const WidgetOptions& options)
{
    if (visible != nullptr && !*visible)
    {
        return false;
    }
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ImId id = ctx.id(label);
    ctx.push_id(id);
    const ImId close_id = ctx.id("close");
    ctx.pop_id();
    const ItemState state = ctx.item(id);
    const ItemState close = visible != nullptr ? ctx.item(close_id) : ItemState{};
    const bool open = toggled_open(ctx, id, default_open, state.clicked && !close.hovered);
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.width = grow();
        box.align_x = Align::Start;
    }
    box.direction = Direction::Row;
    box.align_y = Align::Centre;
    box.gap = 4.0f;
    const uint32_t index = ctx.begin_box(label, box);
    im::paint_surface(ctx.layout().node(index).paint, style, im::interaction_fill(style, state, style.background));
    std::ignore = begin_icon_box(ctx, ImId{}, open ? Icon::ChevronDown : Icon::ChevronRight, state.hovered ? style.text : muted(style.text));
    ctx.end_box();
    title_box(ctx, label, style);
    if (visible != nullptr)
    {
        const uint32_t close_index = begin_icon_box(ctx, close_id, Icon::Cross, close.hovered ? style.text : muted(style.text, 0.6f));
        BoxPaint& paint = ctx.layout().node(close_index).paint;
        paint.has_fill = close.hovered;
        paint.fill = close.held ? style.pressed : style.hover;
        paint.radius = uniform_radius(style.radius);
        ctx.end_box();
    }
    ctx.end_box();
    if (visible != nullptr)
    {
        *visible = !close.clicked;
        return open && *visible;
    }
    return open;
}

} // namespace

bool collapsing_header(std::string_view label, bool default_open, const WidgetOptions& options)
{
    return header_impl(label, nullptr, default_open, options);
}

bool collapsing_header(std::string_view label, bool* visible, bool default_open, const WidgetOptions& options)
{
    return header_impl(label, visible, default_open, options);
}

TreeNodeResult begin_tree_node(std::string_view label, const TreeNodeOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ImId id = ctx.id(label);
    const ItemState state = ctx.item(id);
    const bool open = !options.leaf && toggled_open(ctx, id, options.default_open, state.clicked);
    LayoutStyle row = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        row.padding = { 4.0f, 2.0f, 4.0f, 2.0f };
        row.width = grow();
        row.align_x = Align::Start;
    }
    row.direction = Direction::Row;
    row.align_y = Align::Centre;
    row.gap = 4.0f;
    const uint32_t index = ctx.begin_box(label, row);
    LayoutNode& node = ctx.layout().node(index);
    node.paint.has_fill = options.selected || state.hovered;
    node.paint.fill = state.held ? style.pressed : options.selected ? style.accent : style.hover;
    node.paint.radius = uniform_radius(style.radius);
    std::ignore = begin_icon_box(ctx, ImId{}, options.leaf ? Icon::None : open ? Icon::ChevronDown : Icon::ChevronRight, muted(style.text), 12.0f);
    ctx.end_box();
    title_box(ctx, label, style);
    ctx.end_box();
    if (open)
    {
        LayoutStyle children;
        children.width = grow();
        children.direction = Direction::Column;
        children.padding.left = ctx.gui_theme().indent;
        ctx.begin_box(ImId{}, children);
        ctx.push_id(label);
    }
    return { open, state };
}

void end_tree_node()
{
    GuiContext& ctx = context();
    ctx.pop_id();
    ctx.end_box();
}

void begin_scroll(std::string_view name, const ScrollOptions& options)
{
    GuiContext& ctx = context();
    const ImId id = ctx.id(name);
    const ItemState item = ctx.item(id);
    float max_offset = 0.0f;
    Rect view;
    Vec2f content;
    if (scroll_range(ctx, id, view, content))
    {
        max_offset = math::max(0.0f, content[1] - view.size[1]);
    }
    ScrollState& state = ctx.state<ScrollState>(id);
    state.offset = math::clamp(state.offset, 0.0f, max_offset);
    state.hovered = item.hovered;

    LayoutStyle box;
    box.width = options.width;
    box.height = options.height;
    box.direction = Direction::Column;
    box.gap = options.gap;
    box.overflow = Overflow::Scroll;
    box.scroll_offset = { 0.0f, state.offset };
    box.padding.right = max_offset > 0.0f ? ctx.gui_theme().scrollbar_width : 0.0f;
    ctx.begin_box(name, box);
    ctx.push_id(name);
}

float scroll_offset(std::string_view name)
{
    GuiContext& ctx = context();
    return ctx.state<ScrollState>(ctx.id(name)).offset;
}

void end_scroll()
{
    GuiContext& ctx = context();
    const GuiTheme& theme = ctx.gui_theme();
    const ImId id = ctx.current_id();
    Rect view;
    Vec2f content;
    const bool known = scroll_range(ctx, id, view, content);
    const float max_offset = known ? math::max(0.0f, content[1] - view.size[1]) : 0.0f;
    if (max_offset > 0.0f)
    {
        const ImId thumb_id = ctx.id("thumb");
        const ItemState thumb = ctx.item(thumb_id);
        const ItemDrag drag = ctx.item_drag(thumb_id);
        const float thumb_height = math::clamp(view.size[1] * view.size[1] / content[1], k_min_thumb, view.size[1]);
        const float travel = view.size[1] - thumb_height;
        ScrollState& state = ctx.state<ScrollState>(id);
        const float thumb_y = travel > 0.0f ? travel * state.offset / max_offset : 0.0f;
        if (travel > 0.0f && (drag.started || drag.dragging))
        {
            state.offset = math::clamp(state.offset + drag.delta[1] * max_offset / travel, 0.0f, max_offset);
        }
        LayoutStyle bar;
        bar.width = fixed(theme.scrollbar_width);
        bar.height = fixed(thumb_height);
        bar.floating = { true, AttachPoint::TopRight, AttachPoint::TopRight, FloatTarget::Parent, {}, { 0.0f, thumb_y } };
        const ImStyle& style = ctx.theme().base;
        const uint32_t index = ctx.begin_box(thumb_id, bar);
        BoxPaint& paint = ctx.layout().node(index).paint;
        paint.has_fill = true;
        paint.fill = im::interaction_fill(style, thumb, style.border);
        paint.radius = uniform_radius(theme.scrollbar_width * 0.5f);
        ctx.end_box();
    }
    ScrollState& state = ctx.state<ScrollState>(id);
    if (state.hovered && max_offset > 0.0f && !ctx.wheel_consumed())
    {
        const float wheel = ctx.input().wheel[1];
        const float next = math::clamp(state.offset - wheel * theme.scroll_line_px, 0.0f, max_offset);
        if (wheel != 0.0f && next != state.offset)
        {
            std::ignore = ctx.consume_wheel();
            state.offset = next;
        }
    }
    ctx.pop_id();
    ctx.end_box();
}

TabBarScope::TabBarScope(std::string_view name, uint32_t& selected, const TabBarOptions& options)
    : m_selected(selected)
    , m_style(&im::resolved_style(context(), options))
    , m_closable(options.closable)
{
    GuiContext& ctx = context();
    LayoutStyle strip;
    strip.width = grow();
    strip.gap = 2.0f;
    ctx.begin_box(name, strip);
    ctx.push_id(name);
}

TabBarScope::~TabBarScope()
{
    GuiContext& ctx = context();
    ctx.pop_id();
    ctx.end_box();
}

bool TabBarScope::tab(std::string_view label)
{
    return draw_tab(label, m_closable, nullptr);
}

bool TabBarScope::tab(std::string_view label, bool* open)
{
    if (open != nullptr && !*open)
    {
        ++m_count;
        return false;
    }
    return draw_tab(label, open != nullptr || m_closable, open);
}

bool TabBarScope::draw_tab(std::string_view label, bool closable, bool* open)
{
    GuiContext& ctx = context();
    const ImStyle& style = *m_style;
    const uint32_t index = m_count++;
    IdScope scope(ctx, ctx.index_id(index));
    const ImId tab_id = ctx.id("tab");
    const ItemState tab = ctx.item(tab_id);
    const ItemDrag drag = ctx.item_drag(tab_id);
    LayoutStyle box = im::default_box(style);
    box.gap = style.padding.left * 0.5f;
    const uint32_t node_index = ctx.begin_box(tab_id, box);
    im::paint_surface(ctx.layout().node(node_index).paint, style, im::interaction_fill(style, tab, index == m_selected ? style.accent : style.background));
    ItemState close;
    if (closable)
    {
        text_box(label, style);
        const ImId close_id = ctx.id("close");
        close = ctx.item(close_id);
        const uint32_t mark_index = begin_icon_box(ctx, close_id, Icon::Cross, close.hovered ? style.text : muted(style.text, 0.6f), 14.0f);
        BoxPaint& paint = ctx.layout().node(mark_index).paint;
        paint.has_fill = close.hovered;
        paint.fill = close.held ? style.pressed : style.hover;
        paint.radius = uniform_radius(style.radius);
        ctx.end_box();
    }
    else
    {
        ctx.layout().node(node_index).paint.text = ctx.arena().store(label);
        ctx.layout().node(node_index).paint.text_height = style.text_height;
        ctx.layout().node(node_index).paint.text_colour = style.text;
    }
    ctx.end_box();
    if (tab.pressed && !close.hovered)
    {
        m_result.pressed_index = index;
        m_result.changed = m_result.changed || m_selected != index;
        m_selected = index;
    }
    if (drag.started || drag.dragging || drag.ended)
    {
        m_result.drag = drag;
        m_result.drag_index = index;
    }
    if (close.clicked)
    {
        m_result.closed_index = index;
        if (open != nullptr)
        {
            *open = false;
        }
    }
    return index == m_selected && (open == nullptr || *open);
}

bool splitter(std::string_view name, float& first_size, const SplitterOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.theme().base;
    const ImId id = ctx.id(name);
    const ItemState state = ctx.item(id);
    const ItemDrag drag = ctx.item_drag(id);
    const bool row = options.axis == Direction::Row;
    const uint32_t axis = row ? 0 : 1;
    const float before = first_size;
    if (drag.started || drag.dragging)
    {
        first_size += drag.delta[axis];
    }
    const LayoutNode& parent = ctx.layout().current();
    const LayoutStyle parent_style = parent.style;
    const ImId parent_id = parent.id;
    Rect parent_rect;
    float max_first = std::numeric_limits<float>::max();
    if (is_valid(parent_id) && ctx.layout_rect(parent_id, parent_rect))
    {
        const float padding = row ? parent_style.padding.left + parent_style.padding.right : parent_style.padding.top + parent_style.padding.bottom;
        max_first = parent_rect.size[axis] - padding - parent_style.gap * 2.0f - options.thickness - options.min_second;
    }
    first_size = math::clamp(first_size, options.min_first, math::max(options.min_first, max_first));
    if (state.hovered || state.held)
    {
        ctx.request_cursor(row ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical);
    }
    LayoutStyle bar;
    bar.width = row ? fixed(options.thickness) : grow();
    bar.height = row ? grow() : fixed(options.thickness);
    const uint32_t index = ctx.begin_box(id, bar);
    ctx.layout().node(index).paint.has_fill = true;
    ctx.layout().node(index).paint.fill = state.held ? style.accent : im::interaction_fill(style, state, style.border);
    ctx.end_box();
    return first_size != before;
}

namespace
{

ScrollOptions scroll_options_of(const ListBoxOptions& options)
{
    ScrollOptions scroll;
    static_cast<WidgetOptions&>(scroll) = options;
    scroll.height = options.height;
    return scroll;
}

} // namespace

ListBoxScope::Range ListBoxScope::visible_range(std::string_view name, const ListBoxOptions& options)
{
    if (options.item_count == 0)
    {
        return {};
    }
    GuiContext& ctx = context();
    Rect view;
    const bool laid_out = ctx.layout_rect(ctx.id(name), view);
    const float row = math::max(options.item_height, 1.0f);
    const float view_height = laid_out ? view.size[1] : row * static_cast<float>(k_unknown_list_rows);
    const float offset = scroll_offset(name);
    const uint32_t first = static_cast<uint32_t>(math::max(offset / row, 0.0f));
    const uint32_t last = static_cast<uint32_t>(math::max((offset + view_height) / row, 0.0f)) + 1;
    return { math::min(first > 1 ? first - 1 : 0u, options.item_count), math::min(last + 1, options.item_count) };
}

ListBoxScope::ListBoxScope(std::string_view name, const ListBoxOptions& options)
    : ListBoxScope(name, options, visible_range(name, options))
{
}

ListBoxScope::ListBoxScope(std::string_view name, const ListBoxOptions& options, const Range& range)
    : m_first(range.first)
    , m_last(range.last)
    , m_scroll(name, scroll_options_of(options))
    , m_options(options)
{
    if (options.item_count > 0)
    {
        m_row = im::default_box(im::resolved_style(context(), options));
        m_row.width = grow();
        m_row.height = fixed(options.item_height);
        m_row.align_y = Align::Centre;
        m_options.layout = &m_row;
        spacer(m_first);
    }
}

ListBoxScope::~ListBoxScope()
{
    if (m_options.item_count > 0)
    {
        spacer(m_options.item_count - m_last);
    }
}

void ListBoxScope::spacer(uint32_t rows)
{
    if (rows == 0)
    {
        return;
    }
    LayoutStyle box;
    box.width = grow();
    box.height = fixed(static_cast<float>(rows) * m_options.item_height);
    context().begin_box(ImId{}, box);
    context().end_box();
}

ItemState ListBoxScope::item(std::string_view label, bool selected)
{
    if (m_options.item_count > 0)
    {
        throw Error("ListBoxScope with item_count needs item(index, label, selected)", "loop from first_item() to last_item()");
    }
    IdScope row(context(), context().index_id(m_count++));
    SelectableOptions row_options;
    static_cast<WidgetOptions&>(row_options) = m_options;
    return selectable(label, selected, row_options);
}

ItemState ListBoxScope::item(uint32_t index, std::string_view label, bool selected)
{
    IdScope row(context(), context().index_id(index));
    SelectableOptions row_options;
    static_cast<WidgetOptions&>(row_options) = m_options;
    return selectable(label, selected, row_options);
}

bool filter_matches(std::string_view needle, std::string_view text)
{
    if (needle.size() > text.size())
    {
        return false;
    }
    for (size_t start = 0; start + needle.size() <= text.size(); ++start)
    {
        size_t at = 0;
        while (at < needle.size() && lower(text[start + at]) == lower(needle[at]))
        {
            ++at;
        }
        if (at == needle.size())
        {
            return true;
        }
    }
    return false;
}

} // namespace oryx::gui
