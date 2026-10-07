#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiMenus.h"

namespace oryx::gui
{

namespace
{

// Which menu of a bar or menu list is open, as its id.
struct MenuHost
{
    uint64_t open = 0;
};

struct PopupState
{
    bool open = false;
    Vec2f at{ 0.0f, 0.0f };
};

ImId host_id(GuiContext& ctx)
{
    return ctx.id("menu_host");
}

LayoutStyle popup_box(const ImStyle& style, const Floating& floating, float min_width)
{
    LayoutStyle box;
    box.width = fit(min_width);
    box.direction = Direction::Column;
    box.padding = uniform_insets(2.0f);
    box.channel = k_channel_popup;
    box.floating = floating;
    return box;
}

// Reads the layer's result and opens its box; false (layer already closed) when it asks to close.
bool open_popup_box(GuiContext& ctx, ImId popup, const ImStyle& style, const Floating& floating, float min_width, bool pointer_on_anchor)
{
    const PopupResult result = ctx.begin_popup_layer(popup);
    if (result.closed_by_escape || (result.closed_by_outside && !pointer_on_anchor))
    {
        ctx.end_popup_layer();
        return false;
    }
    const uint32_t index = ctx.begin_box(popup, popup_box(style, floating, min_width));
    im::paint_surface(ctx.layout().node(index).paint, style, style.background);
    return true;
}

Vec2f last_size_of(const GuiContext& ctx, ImId id)
{
    Rect rect;
    return ctx.layout_rect(id, rect) ? rect.size : Vec2f(0.0f, 0.0f);
}

} // namespace

void begin_menu_bar(std::string_view name, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    LayoutStyle bar = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        bar.width = grow();
        bar.padding = { 2.0f, 2.0f, 2.0f, 2.0f };
        bar.gap = 2.0f;
        bar.align_x = Align::Start;
    }
    const uint32_t index = ctx.begin_box(name, bar);
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.has_fill = true;
    paint.fill = style.background;
    ctx.push_id(name);
}

void end_menu_bar()
{
    GuiContext& ctx = context();
    ctx.pop_id();
    ctx.end_box();
}

bool begin_menu(std::string_view label, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ImId id = ctx.id(label);
    const ItemState state = ctx.item(id);
    const ImId host = host_id(ctx);
    const bool top_level = ctx.popup_depth() == 0;
    bool open = false;
    {
        MenuHost& menus = ctx.state<MenuHost>(host);
        menus.open = ctx.menu_close_requested() ? 0 : menus.open;
        if (state.clicked && top_level)
        {
            menus.open = menus.open == id.value ? 0 : id.value;
        }
        else if (state.hovered && (!top_level || menus.open != 0))
        {
            menus.open = id.value;
        }
        open = menus.open == id.value;
    }

    LayoutStyle header = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        header.align_x = Align::Start;
        header.width = top_level ? fit() : grow();
    }
    const uint32_t index = ctx.begin_box(id, header);
    LayoutNode& node = ctx.layout().node(index);
    im::paint_surface(node.paint, style, im::interaction_fill(style, state, open ? style.hover : style.background));
    node.paint.border_width = 0.0f;
    node.paint.text = top_level ? ctx.arena().store(label) : ctx.arena().format("%.*s  >", static_cast<int>(label.size()), label.data());
    node.paint.text_height = style.text_height;
    node.paint.text_colour = style.text;
    ctx.end_box();
    if (!open)
    {
        return false;
    }

    ctx.push_id(label);
    const ImId popup = ctx.id("popup");
    Rect anchor;
    std::ignore = ctx.layout_rect(id, anchor);
    const Vec2f last = last_size_of(ctx, popup);
    Floating floating = im::popup_below(id, anchor, last, ctx.input().surface_size);
    if (!top_level)
    {
        const bool flip = rect_max(anchor)[0] + last[0] > ctx.input().surface_size[0] && anchor.min[0] >= last[0];
        floating.element = flip ? AttachPoint::TopRight : AttachPoint::TopLeft;
        floating.target_point = flip ? AttachPoint::TopLeft : AttachPoint::TopRight;
    }
    if (!open_popup_box(ctx, popup, style, floating, top_level ? 0.0f : 80.0f, state.hovered))
    {
        ctx.pop_id();
        MenuHost& menus = ctx.state<MenuHost>(host);
        menus.open = 0;
        return false;
    }
    return true;
}

void end_menu()
{
    GuiContext& ctx = context();
    ctx.end_box();
    ctx.end_popup_layer();
    ctx.pop_id();
}

ItemState menu_item(std::string_view label, bool selected, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    SelectableOptions row;
    static_cast<WidgetOptions&>(row) = options;
    const ItemState state = selectable(label, selected, row);
    if (state.hovered)
    {
        ctx.state<MenuHost>(host_id(ctx)).open = 0;
    }
    if (state.clicked)
    {
        ctx.request_menu_close();
    }
    return state;
}

void open_popup(std::string_view name)
{
    GuiContext& ctx = context();
    PopupState& state = ctx.state<PopupState>(ctx.id(name));
    state.open = true;
    state.at = ctx.input().pointer.position;
}

void close_popup(std::string_view name)
{
    GuiContext& ctx = context();
    ctx.state<PopupState>(ctx.id(name)).open = false;
}

void close_current_popup()
{
    GuiContext& ctx = context();
    if (is_valid(ctx.popup_id()))
    {
        ctx.state<PopupState>(ctx.popup_id()).open = false;
    }
}

bool begin_popup(std::string_view name, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ImId id = ctx.id(name);
    const PopupState state = ctx.state<PopupState>(id);
    if (!state.open)
    {
        return false;
    }
    const Floating floating = im::popup_at(state.at, last_size_of(ctx, id), ctx.input().surface_size);
    if (!open_popup_box(ctx, id, style, floating, 0.0f, false))
    {
        ctx.state<PopupState>(id).open = false;
        return false;
    }
    ctx.push_id(name);
    return true;
}

void end_popup()
{
    GuiContext& ctx = context();
    ctx.pop_id();
    ctx.end_box();
    ctx.end_popup_layer();
}

bool context_menu(const ItemState& item, std::string_view name, const WidgetOptions& options)
{
    if (item.right_clicked)
    {
        open_popup(name);
    }
    return begin_popup(name, options);
}

void tooltip(const ItemState& item, std::string_view text, float delay)
{
    GuiContext& ctx = context();
    const GuiTheme& theme = ctx.gui_theme();
    if (!item.hovered || item.hovered_seconds < (delay >= 0.0f ? delay : theme.tooltip_delay))
    {
        return;
    }
    const ImStyle& style = theme.base;
    const ImId id = ctx.id("tooltip");
    LayoutStyle box;
    box.padding = style.padding;
    box.channel = k_channel_tooltip;
    box.floating = im::popup_at(ctx.input().pointer.position, last_size_of(ctx, id), ctx.input().surface_size, 14.0f);
    const uint32_t index = ctx.begin_box(id, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    im::paint_surface(paint, style, style.background);
    paint.text = ctx.arena().store(text);
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    ctx.end_box();
}

void toast(std::string_view text, float seconds)
{
    context().add_toast(text, seconds);
}

void show_toasts()
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.theme().base;
    LayoutStyle stack;
    stack.direction = Direction::Column;
    stack.gap = ctx.gui_theme().spacing;
    stack.channel = k_channel_tooltip;
    stack.floating = { true, AttachPoint::BottomRight, AttachPoint::BottomRight, FloatTarget::Root, {}, { -12.0f, -12.0f } };
    bool any = false;
    for (GuiToast& toast : ctx.toasts())
    {
        if (toast.remaining <= 0.0f)
        {
            continue;
        }
        if (!any)
        {
            ctx.begin_box(ctx.id("toasts"), stack);
            any = true;
        }
        LayoutStyle box;
        box.padding = style.padding;
        const uint32_t index = ctx.begin_box(ImId{}, box);
        BoxPaint& paint = ctx.layout().node(index).paint;
        im::paint_surface(paint, style, style.background);
        paint.text = ctx.arena().store(toast.text);
        paint.text_height = style.text_height;
        paint.text_colour = style.text;
        ctx.end_box();
        toast.remaining -= ctx.delta_time();
    }
    if (any)
    {
        ctx.end_box();
    }
}

} // namespace oryx::gui
