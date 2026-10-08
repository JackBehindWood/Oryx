#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiControls.h"

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
    , m_style(&context().role_style(options, &GuiTheme::panel))
{
    GuiContext& ctx = context();
    const GuiTheme& theme = ctx.gui_theme();
    m_label_width = options.label_width >= 0.0f ? options.label_width : theme.label_width;
    m_label_after = side_of(theme, options) == LabelSide::After;
    m_hidden = m_name.starts_with("##");
    LayoutStyle row;
    row.width = options.width;
    row.align_y = Align::Centre;
    row.gap = theme.spacing;
    ctx.begin_box(name, row);
    ctx.push_id(name);
    if (!m_label_after && !m_hidden)
    {
        text_box(m_name, *m_style, m_label_width > 0.0f ? fixed(m_label_width) : fit());
    }
}

FieldScope::~FieldScope()
{
    GuiContext& ctx = context();
    if (m_label_after && !m_hidden)
    {
        text_box(m_name, *m_style, m_label_width > 0.0f ? fixed(m_label_width) : fit());
    }
    ctx.pop_id();
    ctx.end_box();
}

void text_coloured(std::string_view text, const Colour& colour, const WidgetOptions& options)
{
    ImStyle style = context().role_style(options, &GuiTheme::panel);
    style.text = colour;
    WidgetOptions coloured = options;
    coloured.style = &style;
    label(text, coloured);
}

void bullet(std::string_view text, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
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
    const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
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
    LayoutStyle box = im::default_box(context().role_style(options, &GuiTheme::button));
    box.padding = { 4.0f, 1.0f, 4.0f, 1.0f };
    WidgetOptions small = options;
    small.layout = options.layout != nullptr ? options.layout : &box;
    return button(text, small);
}

namespace
{

Colour toward(const Colour& from, const Colour& to, float t)
{
    return lerp(from, to, t);
}

// The box of a checkbox or radio: a ring-less field that takes the accent when set, with a check mark or a dot.
void choice_mark(GuiContext& ctx, const ImStyle& style, const ItemState& state, bool set, bool round)
{
    const float side = style.text_height;
    LayoutStyle box = square_box(side);
    box.align_x = Align::Centre;
    box.align_y = Align::Centre;
    const uint32_t index = ctx.begin_box(ImId{}, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    const bool lit = state.hovered || state.held;
    const Colour rest_border = lit ? toward(style.border, style.text, 0.45f) : style.border;
    paint.has_fill = true;
    paint.radius = uniform_radius(round ? side * 0.5f : style.radius);
    paint.border_width = math::max(1.0f, style.border_width);
    if (round)
    {
        paint.fill = im::interaction_fill(style, state, style.background);
        paint.border = set ? im::keep_fill(state, style.accent) : rest_border;
        if (set)
        {
            LayoutStyle dot = square_box(side * 0.48f);
            const uint32_t dot_index = ctx.begin_box(ImId{}, dot);
            BoxPaint& dot_paint = ctx.layout().node(dot_index).paint;
            dot_paint.has_fill = true;
            dot_paint.fill = im::keep_fill(state, style.accent);
            dot_paint.radius = uniform_radius(side);
            ctx.end_box();
        }
    }
    else if (set)
    {
        paint.fill = im::keep_fill(state, style.accent);
        paint.border = paint.fill;
        paint.icon = Icon::Check;
        paint.icon_colour = style.on_accent;
        paint.icon_size = side * 0.72f;
        paint.icon_thickness = 2.0f;
    }
    else
    {
        paint.fill = im::interaction_fill(style, state, style.background);
        paint.border = rest_border;
    }
    ctx.end_box();
}

FieldOptions label_hit(const FieldOptions& options)
{
    FieldOptions fit_row = options;
    fit_row.width = fit();
    return fit_row;
}

} // namespace

bool checkbox(std::string_view label, bool& value, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
    const ItemState state = ctx.item(ctx.id(label));
    {
        FieldScope field(label, label_hit(options));
        choice_mark(ctx, style, state, value, false);
    }
    if (state.hovered)
    {
        ctx.request_cursor(CursorShape::Hand);
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
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
    const ItemState state = ctx.item(ctx.id(label));
    {
        FieldScope field(label, label_hit(options));
        choice_mark(ctx, style, state, value == option, true);
    }
    if (state.hovered)
    {
        ctx.request_cursor(CursorShape::Hand);
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
    const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
    const ItemState state = ctx.item(ctx.id(label));
    LayoutStyle box = im::widget_box(style, options);
    if (options.layout == nullptr)
    {
        box.width = options.full_width ? grow() : fit();
        box.align_x = Align::Start;
    }
    const uint32_t index = ctx.begin_box(label, box);
    LayoutNode& node = ctx.layout().node(index);
    node.paint.has_fill = selected || state.hovered || state.held;
    node.paint.fill = selected ? im::keep_fill(state, style.selected) : state.held ? style.pressed : style.hover;
    node.paint.radius = uniform_radius(style.radius);
    im::paint_text(node.paint, node, style, TextAlign::Left, true);
    ctx.end_box();
    return state;
}

bool select_on_press(const ItemState& item, uint32_t index, Selection& selection, uint64_t* words, uint32_t count)
{
    return item.pressed && select_click(selection, words, count, index, context().input().keys);
}

void badge(std::string_view text, const Colour& colour, const WidgetOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
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
    const ImStyle& style = context().role_style(options, &GuiTheme::field);
    FieldScope field(label, options);
    leaf_surface(context(), square_box(style.text_height), style, colour, style.radius);
}

void progress(std::string_view label, float fraction, std::string_view text, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
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
