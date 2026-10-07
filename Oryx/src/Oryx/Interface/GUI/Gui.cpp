#include "oxpch.h"
#include "Oryx/Interface/GUI/Gui.h"

namespace oryx::gui
{

namespace
{

GuiContext& active()
{
    return ActiveContext<GuiContext>::require();
}

} // namespace

GuiContext& context()
{
    return active();
}

GuiId id(std::string_view label)
{
    return GuiId(active().id(label));
}

void push_id(std::string_view label)
{
    active().push_id(label);
}

void pop_id()
{
    active().pop_id();
}

ItemState item(GuiId id, const Rect& rect)
{
    return active().item(id.im(), rect);
}

GuiIo io()
{
    const GuiContext& context = active();
    return { context.wants_mouse(), context.wants_keyboard(), context.delta_time(), context.frame(), context.input().surface, context.input().pointer };
}

const GuiTheme& theme()
{
    return active().gui_theme();
}

void request_cursor(CursorShape cursor)
{
    active().request_cursor(cursor);
}

ItemDrag item_drag(GuiId id)
{
    return active().item_drag(id.im());
}

ItemState begin_widget(std::string_view name, const LayoutStyle& style)
{
    return im::begin_widget(active(), name, style);
}

void end_widget()
{
    im::end_widget(active());
}

im::CanvasArea canvas(std::string_view name, Sizing width, Sizing height, const im::CanvasOptions& options)
{
    return im::canvas(active(), name, width, height, options);
}

float spacing()
{
    return active().gui_theme().spacing;
}

void label(std::string_view text, const WidgetOptions& options)
{
    im::label(active(), text, options);
}

ItemState button(std::string_view text, const WidgetOptions& options)
{
    return im::button(active(), text, options);
}

bool toggle(std::string_view text, bool& value, const WidgetOptions& options)
{
    return im::toggle(active(), text, value, options);
}

void begin_panel(std::string_view name, const WidgetOptions& options)
{
    im::begin_panel(active(), name, options);
}

void end_panel()
{
    im::end_panel(active());
}

void begin_row(std::string_view name, const RowOptions& options)
{
    im::begin_row(active(), name, options);
}

void end_row()
{
    im::end_row(active());
}

void begin_column(std::string_view name, const RowOptions& options)
{
    im::begin_column(active(), name, options);
}

void end_column()
{
    im::end_column(active());
}

void spacer(float weight)
{
    im::spacer(active(), weight);
}

void separator(const WidgetOptions& options)
{
    im::separator(active(), options);
}

void status_line(std::string_view text, const StatusOptions& options)
{
    im::status_line(active(), text, options);
}

uint32_t begin_box(std::string_view label, const LayoutStyle& style)
{
    return active().begin_box(label, style);
}

void end_box()
{
    active().end_box();
}

LayoutStyle anchored(AttachPoint at, Sizing width, Sizing height, const Vec2f& offset)
{
    LayoutStyle style;
    style.width = width;
    style.height = height;
    style.floating = { true, at, at, FloatTarget::Root, {}, offset };
    return style;
}

} // namespace oryx::gui
