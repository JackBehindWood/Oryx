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

// Scroll range from the latest solve; false before the box has been laid out.
bool scroll_range(GuiContext& ctx, ImId id, Rect& view, Vec2f& content)
{
    return ctx.layout_rect(id, view) && ctx.layout().content_size_of(id, content);
}

} // namespace

bool collapsing_header(std::string_view label, bool default_open, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ImId id = ctx.id(label);
    const ItemState state = ctx.item(id);
    const bool open = toggled_open(ctx, id, default_open, state.clicked);
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.width = grow();
        box.align_x = Align::Start;
    }
    const uint32_t index = ctx.begin_box(label, box);
    LayoutNode& node = ctx.layout().node(index);
    im::paint_surface(node.paint, style, im::interaction_fill(style, state, style.background));
    im::paint_text(node.paint, node, style, TextAlign::Left, true);
    node.paint.text = ctx.arena().format("%s %.*s", open ? "v" : ">", static_cast<int>(label.size()), label.data());
    ctx.end_box();
    return open;
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
    const uint32_t index = ctx.begin_box(label, row);
    LayoutNode& node = ctx.layout().node(index);
    node.paint.has_fill = options.selected || state.hovered;
    node.paint.fill = state.held ? style.pressed : options.selected ? style.accent : style.hover;
    node.paint.radius = uniform_radius(style.radius);
    im::paint_text(node.paint, node, style, TextAlign::Left, true);
    node.paint.text = ctx.arena().format("%s %.*s", options.leaf ? " " : open ? "v" : ">", static_cast<int>(label.size()), label.data());
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

TabBarResult tab_bar(std::string_view name, std::span<const std::string_view> labels, uint32_t& selected, const TabBarOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    TabBarResult result;
    LayoutStyle strip;
    strip.width = grow();
    strip.gap = 2.0f;
    ctx.begin_box(name, strip);
    ctx.push_id(name);
    for (uint32_t index = 0; index < labels.size(); ++index)
    {
        IdScope scope(ctx, ctx.index_id(index));
        const ImId tab_id = ctx.id("tab");
        const ItemState tab = ctx.item(tab_id);
        const ItemDrag drag = ctx.item_drag(tab_id);
        LayoutStyle box = im::default_box(style);
        box.gap = style.padding.left * 0.5f;
        const uint32_t node_index = ctx.begin_box(tab_id, box);
        im::paint_surface(ctx.layout().node(node_index).paint, style, im::interaction_fill(style, tab, index == selected ? style.accent : style.background));
        ItemState close;
        if (options.closable)
        {
            text_box(labels[index], style);
            const ImId close_id = ctx.id("close");
            close = ctx.item(close_id);
            LayoutStyle mark;
            mark.padding = { 3.0f, 0.0f, 3.0f, 0.0f };
            const uint32_t mark_index = ctx.begin_box(close_id, mark);
            BoxPaint& paint = ctx.layout().node(mark_index).paint;
            paint.has_fill = close.hovered;
            paint.fill = style.hover;
            paint.radius = uniform_radius(style.radius);
            paint.text = "x";
            paint.text_height = style.text_height;
            paint.text_colour = style.text;
            ctx.end_box();
        }
        else
        {
            ctx.layout().node(node_index).paint.text = ctx.arena().store(labels[index]);
            ctx.layout().node(node_index).paint.text_height = style.text_height;
            ctx.layout().node(node_index).paint.text_colour = style.text;
        }
        ctx.end_box();
        if (tab.pressed && !close.hovered)
        {
            result.pressed_index = index;
            result.changed = result.changed || selected != index;
            selected = index;
        }
        if (drag.started || drag.dragging || drag.ended)
        {
            result.drag = drag;
            result.drag_index = index;
        }
        if (close.clicked)
        {
            result.closed_index = index;
        }
    }
    ctx.pop_id();
    ctx.end_box();
    return result;
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

bool list_box(std::string_view name, std::span<const std::string_view> items, int32_t& selected, const ListBoxOptions& options)
{
    GuiContext& ctx = context();
    ScrollOptions scroll;
    static_cast<WidgetOptions&>(scroll) = options;
    scroll.height = options.height;
    bool changed = false;
    ScrollScope region(name, scroll);
    for (uint32_t index = 0; index < items.size(); ++index)
    {
        IdScope row(ctx, ctx.index_id(index));
        SelectableOptions row_options;
        static_cast<WidgetOptions&>(row_options) = options;
        const ItemState state = selectable(items[index], static_cast<int32_t>(index) == selected, row_options);
        if (state.clicked && selected != static_cast<int32_t>(index))
        {
            selected = static_cast<int32_t>(index);
            changed = true;
        }
    }
    return changed;
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
