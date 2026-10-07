#include "oxpch.h"
#include "Oryx/Interface/Canvas/ImWidgets.h"

namespace oryx::im
{

namespace
{

const ImStyle& resolve_style(const ImContext& context, const WidgetOptions& options)
{
    return options.style != nullptr ? *options.style : style_for(context.theme(), options.variant);
}

LayoutStyle default_box(const ImStyle& style)
{
    LayoutStyle box;
    box.padding = style.padding;
    box.align_x = Align::Centre;
    box.align_y = Align::Centre;
    return box;
}

LayoutStyle box_for(const ImStyle& style, const WidgetOptions& options)
{
    return options.layout != nullptr ? *options.layout : default_box(style);
}

void set_text(BoxPaint& paint, const LayoutNode& node, const ImStyle& style, TextAlign align, bool ellipsis)
{
    paint.text = node.name;
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    paint.text_align = align;
    paint.ellipsis = ellipsis;
}

void set_surface(BoxPaint& paint, const ImStyle& style, const Colour& fill)
{
    paint.has_fill = true;
    paint.fill = fill;
    paint.radius = uniform_radius(style.radius);
    paint.border_width = style.border_width;
    paint.border = style.border;
}

Colour interaction_fill(const ImStyle& style, const ItemState& state, const Colour& rest)
{
    return state.held ? style.pressed : state.hovered ? style.hover : rest;
}

// A leaf box that reacts to the pointer; the colour of `rest` shows while idle.
ItemState interactive_box(ImContext& context, std::string_view text, const WidgetOptions& options, const Colour* rest)
{
    const ImStyle& style = resolve_style(context, options);
    const ItemState state = context.item(context.id(text));
    const uint32_t index = context.begin_box(text, box_for(style, options));
    LayoutNode& node = context.layout().node(index);
    set_surface(node.paint, style, interaction_fill(style, state, rest != nullptr ? *rest : style.background));
    set_text(node.paint, node, style, TextAlign::Centre, true);
    context.end_box();
    return state;
}

} // namespace

void label(ImContext& context, std::string_view text, const WidgetOptions& options)
{
    const ImStyle& style = resolve_style(context, options);
    LayoutStyle box = box_for(style, options);
    if (options.layout == nullptr)
    {
        box.align_x = Align::Start;
    }
    const uint32_t index = context.begin_box(text, box);
    LayoutNode& node = context.layout().node(index);
    set_text(node.paint, node, style, TextAlign::Left, false);
    context.end_box();
}

ItemState button(ImContext& context, std::string_view text, const WidgetOptions& options)
{
    return interactive_box(context, text, options, nullptr);
}

bool toggle(ImContext& context, std::string_view text, bool& value, const WidgetOptions& options)
{
    const ImStyle& style = resolve_style(context, options);
    const ItemState state = interactive_box(context, text, options, value ? &style.accent : nullptr);
    if (state.clicked)
    {
        value = !value;
    }
    return state.clicked;
}

void begin_panel(ImContext& context, std::string_view name, const WidgetOptions& options)
{
    const ImStyle& style = resolve_style(context, options);
    LayoutStyle box = box_for(style, options);
    if (options.layout == nullptr)
    {
        box.direction = Direction::Column;
        box.align_x = Align::Start;
        box.align_y = Align::Start;
    }
    box.overflow = Overflow::Clip;
    const uint32_t index = context.begin_box(name, box);
    set_surface(context.layout().node(index).paint, style, style.background);
    context.push_id(name);
}

void end_panel(ImContext& context)
{
    context.pop_id();
    context.end_box();
}

void status_line(ImContext& context, std::string_view text, const StatusOptions& options)
{
    const ImStyle& style = resolve_style(context, options);
    LayoutStyle box = box_for(style, options);
    if (options.layout == nullptr)
    {
        const float margin = style.padding.bottom * 2.0f;
        const float direction = options.at == AttachPoint::TopLeft || options.at == AttachPoint::TopCentre || options.at == AttachPoint::TopRight ? 1.0f : -1.0f;
        box.floating = { true, options.at, options.at, FloatTarget::Root, {}, { 0.0f, direction * margin } };
    }
    const uint32_t index = context.begin_box(text, box);
    set_text(context.layout().node(index).paint, context.layout().node(index), style, TextAlign::Centre, false);
    context.end_box();
}

} // namespace oryx::im
