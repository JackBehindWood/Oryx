#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

namespace
{

LayoutStyle square_box(float side)
{
    LayoutStyle box;
    box.width = fixed(side);
    box.height = fixed(side);
    return box;
}

void leaf_surface(GuiContext& ctx, const LayoutStyle& box, const ImStyle& style, const Colour& fill, float radius)
{
    const uint32_t index = ctx.begin_box(ImId{}, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    im::paint_surface(paint, style, fill);
    paint.radius = uniform_radius(radius);
    ctx.end_box();
}

LabelSide side_of(const GuiTheme& theme, const FieldOptions& options)
{
    return options.label_side == LabelSide::Default ? theme.label_side : options.label_side;
}

} // namespace

uint32_t text_box(std::string_view text, const ImStyle& style, Sizing width, TextAlign align)
{
    GuiContext& ctx = context();
    LayoutStyle box;
    box.width = width;
    const uint32_t index = ctx.begin_box(ImId{}, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.text = ctx.arena().store(text);
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    paint.text_align = align;
    ctx.end_box();
    return index;
}

void overlay_text(std::string_view text, const ImStyle& style)
{
    GuiContext& ctx = context();
    LayoutStyle box;
    box.width = grow();
    box.height = grow();
    box.align_x = Align::Centre;
    box.align_y = Align::Centre;
    box.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Parent, {}, { 0.0f, 0.0f } };
    const uint32_t index = ctx.begin_box(ImId{}, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.text = ctx.arena().store(text);
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    paint.text_align = TextAlign::Centre;
    ctx.end_box();
}

FieldScope::FieldScope(std::string_view name, const FieldOptions& options)
    : m_name(name)
    , m_style(&im::resolved_style(context(), options))
{
    GuiContext& ctx = context();
    const GuiTheme& theme = ctx.gui_theme();
    m_label_width = options.label_width >= 0.0f ? options.label_width : theme.label_width;
    m_label_after = side_of(theme, options) == LabelSide::After;
    LayoutStyle row;
    row.width = options.width;
    row.align_y = Align::Centre;
    row.gap = theme.spacing;
    ctx.begin_box(name, row);
    ctx.push_id(name);
    if (!m_label_after)
    {
        text_box(m_name, *m_style, m_label_width > 0.0f ? fixed(m_label_width) : fit());
    }
}

FieldScope::~FieldScope()
{
    GuiContext& ctx = context();
    if (m_label_after)
    {
        text_box(m_name, *m_style, m_label_width > 0.0f ? fixed(m_label_width) : fit());
    }
    ctx.pop_id();
    ctx.end_box();
}

void text_coloured(std::string_view text, const Colour& colour, const WidgetOptions& options)
{
    ImStyle style = im::resolved_style(context(), options);
    style.text = colour;
    WidgetOptions coloured = options;
    coloured.style = &style;
    label(text, coloured);
}

void bullet(std::string_view text, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    LayoutStyle row;
    row.align_y = Align::Centre;
    row.gap = ctx.gui_theme().spacing;
    ctx.begin_box(text, row);
    ctx.push_id(text);
    LayoutStyle dot = square_box(style.text_height * 0.35f);
    const uint32_t index = ctx.begin_box(ImId{}, dot);
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.has_fill = true;
    paint.fill = style.text;
    paint.radius = uniform_radius(style.text_height);
    ctx.end_box();
    text_box(text, style);
    ctx.pop_id();
    ctx.end_box();
}

void key_value(std::string_view key, std::string_view value, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    LayoutStyle row;
    row.width = grow();
    row.align_y = Align::Centre;
    row.gap = ctx.gui_theme().spacing;
    ctx.begin_box(key, row);
    ctx.push_id(key);
    text_box(key, style);
    spacer();
    text_box(value, style, fit(), TextAlign::Right);
    ctx.pop_id();
    ctx.end_box();
}

ItemState small_button(std::string_view text, const WidgetOptions& options)
{
    LayoutStyle box = im::default_box(im::resolved_style(context(), options));
    box.padding = { 4.0f, 1.0f, 4.0f, 1.0f };
    WidgetOptions small = options;
    small.layout = options.layout != nullptr ? options.layout : &box;
    return button(text, small);
}

bool checkbox(std::string_view label, bool& value, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ItemState state = ctx.item(ctx.id(label));
    {
        FieldScope field(label, options);
        leaf_surface(ctx, square_box(style.text_height), style, im::interaction_fill(style, state, value ? style.accent : style.background), style.radius);
    }
    if (state.clicked)
    {
        value = !value;
    }
    return state.clicked;
}

bool radio(std::string_view label, int32_t& value, int32_t option, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ItemState state = ctx.item(ctx.id(label));
    {
        FieldScope field(label, options);
        leaf_surface(ctx, square_box(style.text_height), style, im::interaction_fill(style, state, value == option ? style.accent : style.background), style.text_height);
    }
    const bool changed = state.clicked && value != option;
    if (state.clicked)
    {
        value = option;
    }
    return changed;
}

ItemState selectable(std::string_view label, bool selected, const SelectableOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    const ItemState state = ctx.item(ctx.id(label));
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.width = options.full_width ? grow() : fit();
        box.align_x = Align::Start;
    }
    const uint32_t index = ctx.begin_box(label, box);
    LayoutNode& node = ctx.layout().node(index);
    const bool lit = selected || state.hovered;
    node.paint.has_fill = lit;
    node.paint.fill = state.held ? style.pressed : selected ? style.accent : style.hover;
    node.paint.radius = uniform_radius(style.radius);
    im::paint_text(node.paint, node, style, TextAlign::Left, true);
    ctx.end_box();
    return state;
}

void badge(std::string_view text, const Colour& colour, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.padding = { 6.0f, 1.0f, 6.0f, 1.0f };
    }
    const uint32_t index = ctx.begin_box(text, box);
    LayoutNode& node = ctx.layout().node(index);
    node.paint.has_fill = true;
    node.paint.fill = colour;
    node.paint.radius = uniform_radius(style.text_height);
    im::paint_text(node.paint, node, style, TextAlign::Centre, false);
    ctx.end_box();
}

void colour_swatch(std::string_view label, const Colour& colour, const FieldOptions& options)
{
    const ImStyle& style = im::resolved_style(context(), options);
    FieldScope field(label, options);
    leaf_surface(context(), square_box(style.text_height), style, colour, style.radius);
}

void progress(std::string_view label, float fraction, std::string_view text, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    FieldScope field(label, options);
    LayoutStyle track;
    track.width = grow(1.0f, 80.0f);
    track.height = fixed(style.text_height);
    const uint32_t index = ctx.begin_box(ImId{}, track);
    im::paint_surface(ctx.layout().node(index).paint, style, style.background);
    LayoutStyle fill;
    fill.width = percent(math::saturate(fraction));
    fill.height = grow();
    const uint32_t fill_index = ctx.begin_box(ImId{}, fill);
    BoxPaint& paint = ctx.layout().node(fill_index).paint;
    paint.has_fill = true;
    paint.fill = style.accent;
    paint.radius = uniform_radius(style.radius);
    ctx.end_box();
    if (!text.empty())
    {
        overlay_text(text, style);
    }
    ctx.end_box();
}

} // namespace oryx::gui
