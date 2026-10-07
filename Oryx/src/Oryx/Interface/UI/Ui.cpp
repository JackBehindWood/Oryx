#include "oxpch.h"
#include "Oryx/Interface/UI/Ui.h"

namespace oryx::ui
{

namespace
{

UiContext& active()
{
    return ActiveContext<UiContext>::require();
}

} // namespace

UiId id(std::string_view label)
{
    return UiId(active().id(label));
}

void push_id(std::string_view label)
{
    active().push_id(label);
}

void pop_id()
{
    active().pop_id();
}

ItemState item(UiId id, const Rect& rect)
{
    return active().item(id.im(), rect);
}

float spacing()
{
    return active().ui_theme().spacing;
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

ItemState image(std::string_view name, ImageHandle image, const ImageOptions& options)
{
    return im::image(active(), name, image, options);
}

ItemState image_button(std::string_view name, ImageHandle image, const ImageButtonOptions& options)
{
    return im::image_button(active(), name, image, options);
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

void icon(Icon icon, const IconOptions& options)
{
    im::icon(active(), icon, options);
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
    UiContext& context = active();
    StatusOptions resolved = options;
    if (resolved.style == nullptr && resolved.variant.empty())
    {
        resolved.style = &context.ui_theme().status;
    }
    im::status_line(context, text, resolved);
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

} // namespace oryx::ui
