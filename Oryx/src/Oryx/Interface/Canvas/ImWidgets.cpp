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

LayoutStyle stack_style(Direction direction, const RowOptions& options)
{
    LayoutStyle box;
    box.width = options.width;
    box.height = options.height;
    box.direction = direction;
    box.padding = options.padding;
    box.gap = options.gap;
    (direction == Direction::Row ? box.align_y : box.align_x) = options.align;
    return box;
}

void begin_stack(ImContext& context, std::string_view name, Direction direction, const RowOptions& options)
{
    context.begin_box(name, stack_style(direction, options));
    context.push_id(name);
}

void end_stack(ImContext& context)
{
    context.pop_id();
    context.end_box();
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

void begin_row(ImContext& context, std::string_view name, const RowOptions& options)
{
    begin_stack(context, name, Direction::Row, options);
}

void end_row(ImContext& context)
{
    end_stack(context);
}

void begin_column(ImContext& context, std::string_view name, const RowOptions& options)
{
    begin_stack(context, name, Direction::Column, options);
}

void end_column(ImContext& context)
{
    end_stack(context);
}

void spacer(ImContext& context, float weight)
{
    LayoutStyle box;
    box.width = grow(weight);
    box.height = grow(weight);
    context.begin_box(ImId{}, box);
    context.end_box();
}

void separator(ImContext& context, const WidgetOptions& options)
{
    const ImStyle& style = resolve_style(context, options);
    LayoutStyle box;
    if (options.layout != nullptr)
    {
        box = *options.layout;
    }
    else
    {
        const bool across_column = context.layout().open_depth() == 0 || context.layout().current().style.direction == Direction::Column;
        const float thickness = std::max(style.border_width, 1.0f);
        box.width = across_column ? grow() : fixed(thickness);
        box.height = across_column ? fixed(thickness) : grow();
    }
    const uint32_t index = context.begin_box(ImId{}, box);
    BoxPaint& paint = context.layout().node(index).paint;
    paint.has_fill = true;
    paint.fill = style.border;
    context.end_box();
}

ItemState begin_widget(ImContext& context, std::string_view name, const LayoutStyle& style)
{
    const ItemState state = context.item(context.id(name));
    context.begin_box(name, style);
    context.push_id(name);
    return state;
}

void end_widget(ImContext& context)
{
    context.pop_id();
    context.end_box();
}

CanvasArea::~CanvasArea()
{
    m_draw.pop_clip();
    m_draw.set_channel(m_previous_channel);
}

CanvasArea canvas(ImContext& context, std::string_view name, Sizing width, Sizing height, const CanvasOptions& options)
{
    LayoutStyle box;
    box.width = width;
    box.height = height;
    const ImId id = context.id(name);
    const ItemState item = context.item(id);
    context.begin_box(name, box);
    context.end_box();

    DrawList& draw = context.draw_list();
    Painter painter = context.painter(context.input().scale);
    const uint32_t previous_channel = draw.current_channel();
    draw.split_channels(std::max(draw.channel_count(), options.channel + 1));
    draw.set_channel(options.channel);
    Rect rect;
    std::ignore = context.layout_rect(id, rect);
    Rect clip = unbounded_rect();
    std::ignore = context.layout().clip_of(id, clip);
    draw.push_clip(intersect(rect, clip));

    const ItemDrag drag = context.item_drag(id);
    const Vec2f wheel = item.hovered ? context.consume_wheel() : Vec2f(0.0f, 0.0f);
    return CanvasArea(context, std::move(painter), previous_channel, rect, item, drag, context.input().pointer.position - rect.min, wheel);
}

void status_line(ImContext& context, std::string_view text, const StatusOptions& options)
{
    const ImStyle& style = resolve_style(context, options);
    LayoutStyle box = box_for(style, options);
    if (options.layout == nullptr)
    {
        const float margin = options.margin >= 0.0f ? options.margin : style.padding.bottom * 2.0f;
        const float direction = options.at == AttachPoint::TopLeft || options.at == AttachPoint::TopCentre || options.at == AttachPoint::TopRight ? 1.0f : -1.0f;
        box.floating = { true, options.at, options.at, FloatTarget::Root, {}, { 0.0f, direction * margin } };
    }
    const uint32_t index = context.begin_box(text, box);
    set_text(context.layout().node(index).paint, context.layout().node(index), style, TextAlign::Centre, false);
    context.end_box();
}

} // namespace oryx::im
