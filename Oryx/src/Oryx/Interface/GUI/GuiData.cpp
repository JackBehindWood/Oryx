#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiData.h"

namespace oryx::gui
{

namespace
{

constexpr float k_line_thickness = 1.5f;
constexpr float k_frame_inset = 2.0f;
constexpr float k_label_scale = 0.75f;
constexpr float k_chip_pad = 3.0f;
constexpr float k_min_bar_gap_cell = 4.0f;

float nan_value()
{
    return std::numeric_limits<float>::quiet_NaN();
}

// Dark text on light fills and the reverse.
Colour contrasting(const Colour& fill)
{
    const float luminance = 0.299f * fill.r + 0.587f * fill.g + 0.114f * fill.b;
    return luminance > 0.55f ? Colour{ 0.05f, 0.05f, 0.07f, 1.0f } : Colour{ 0.95f, 0.95f, 0.97f, 1.0f };
}

// The column's samples reduced to what is drawn: where its smallest and largest value sit.
struct Bucket
{
    int32_t column = 0;
    uint32_t count = 0;
    uint32_t min_index = 0;
    uint32_t max_index = 0;
    float min = 0.0f;
    float max = 0.0f;

    void add(int32_t at_column, uint32_t index, float value)
    {
        if (count == 0)
        {
            column = at_column;
            min_index = index;
            max_index = index;
            min = value;
            max = value;
        }
        else
        {
            if (value < min)
            {
                min = value;
                min_index = index;
            }
            if (value > max)
            {
                max = value;
                max_index = index;
            }
        }
        ++count;
    }
};

// The line the series is drawn with, a point at a time.
struct Pen
{
    bool down = false;
    Vec2f last{ 0.0f, 0.0f };
    uint32_t emitted = 0;
};

void resolve_y(float seen_min, float seen_max, bool bars, float fixed_min, float fixed_max, float& out_min, float& out_max)
{
    float low = seen_min;
    float high = seen_max;
    if (!(low <= high))
    {
        low = 0.0f;
        high = 1.0f;
    }
    if (bars)
    {
        low = math::min(low, 0.0f);
        high = math::max(high, 0.0f);
    }
    if (high - low < math::EPSILON<float>)
    {
        low -= 0.5f;
        high += 0.5f;
    }
    else if (!bars)
    {
        const float pad = (high - low) * 0.05f;
        low -= pad;
        high += pad;
    }
    out_min = std::isfinite(fixed_min) ? fixed_min : low;
    out_max = std::isfinite(fixed_max) ? fixed_max : high;
    if (out_max - out_min < math::EPSILON<float>)
    {
        out_max = out_min + 1.0f;
    }
}

} // namespace

void bar(std::string_view label, float value, float max, const BarOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& base = ctx.role_style(options, &GuiTheme::field);
    const float fraction = max > math::EPSILON<float> ? math::saturate(value / max) : 0.0f;
    ImStyle coloured = base;
    if (options.scale != nullptr)
    {
        coloured.accent = evaluate(*options.scale, value);
    }
    FieldOptions field = options;
    field.style = &coloured;
    std::string_view text;
    if (options.show_value)
    {
        text = options.value_text.empty() ? format_number(options.format, value, ctx.arena()) : options.value_text;
    }
    progress(label, fraction, text, field);
}

PlotScope::PlotScope(std::string_view name, const PlotOptions& options)
    : m_context(context())
    , m_style(&m_context.role_style(options, &GuiTheme::panel))
    , m_options(options)
    , m_id(m_context.id(name))
    , m_previous(m_context.state<Memory>(m_id))
    , m_canvas(im::canvas(m_context, name, options.width, options.height))
{
    const Rect& rect = m_canvas.rect;
    if (m_options.frame && !is_empty(rect))
    {
        const CornerRadius radius = uniform_radius(m_style->radius);
        m_canvas.painter.fill_rounded_rect(rect, radius, m_style->background);
        m_canvas.painter.border(rect, radius, m_style->border_width, m_style->border);
    }
    m_inner = m_options.frame ? inset(rect, uniform_insets(k_frame_inset)) : rect;
    const float text_height = m_style->text_height * k_label_scale;
    float header = 0.0f;
    float gutter = 0.0f;
    if (m_options.legend && m_previous.series > 0)
    {
        header = text_height + 2.0f * k_chip_pad;
    }
    if (m_options.range_labels && m_previous.valid)
    {
        float low = 0.0f;
        float high = 1.0f;
        resolve_y(m_previous.y_min, m_previous.y_max, m_previous.bars, m_options.y_min, m_options.y_max, low, high);
        const NumberFormat format;
        const float widest = math::max(m_canvas.painter.text_width(format_number(format, high, m_context.arena()), text_height), m_canvas.painter.text_width(format_number(format, low, m_context.arena()), text_height));
        gutter = widest + 2.0f * k_chip_pad;
    }
    header = math::min(header, m_inner.size[1]);
    gutter = math::min(gutter, m_inner.size[0]);
    m_data = { { m_inner.min[0] + gutter, m_inner.min[1] + header }, { m_inner.size[0] - gutter, m_inner.size[1] - header } };
    if (!is_empty(m_data))
    {
        m_canvas.painter.list().push_clip(m_data);
        m_clipped = true;
    }
}

PlotScope::~PlotScope()
{
    Painter& painter = m_canvas.painter;
    if (m_clipped)
    {
        painter.list().pop_clip();
    }
    const PlotArea plot = area();
    const float text_height = m_style->text_height * k_label_scale;
    const float row = text_height + 2.0f * k_chip_pad;
    Colour muted = m_style->text;
    muted.a = 0.7f;
    Colour chip = m_style->background;
    chip.a = 0.9f;
    const CornerRadius chip_radius = uniform_radius(m_style->radius);
    if (!is_empty(m_canvas.rect))
    {
        const float gutter = m_data.min[0] - m_inner.min[0];
        if (m_options.range_labels && gutter > 0.0f)
        {
            const NumberFormat format;
            const Rect top = { { m_inner.min[0], m_data.min[1] }, { gutter - k_chip_pad, row } };
            painter.text(top, format_number(format, plot.y_max, m_context.arena()), { text_height, muted, TextAlign::Right, false });
            if (m_data.size[1] >= 2.0f * row + 2.0f)
            {
                const Rect bottom = { { m_inner.min[0], m_data.min[1] + m_data.size[1] - row }, { gutter - k_chip_pad, row } };
                painter.text(bottom, format_number(format, plot.y_min, m_context.arena()), { text_height, muted, TextAlign::Right, false });
            }
        }
        const float header = m_data.min[1] - m_inner.min[1];
        if (m_options.legend && m_series > 0 && header > 0.0f)
        {
            const float swatch = text_height * 0.8f;
            const float limit = m_inner.min[0] + m_inner.size[0];
            uint32_t shown = 0;
            float used = k_chip_pad;
            for (; shown < m_series; ++shown)
            {
                const float entry = swatch + 3.0f + painter.text_width(m_entries[shown].name, text_height) + 8.0f;
                if (m_inner.min[0] + used + entry > limit)
                {
                    break;
                }
                used += entry;
            }
            if (shown > 0)
            {
                painter.fill_rounded_rect({ m_inner.min, { used, header } }, chip_radius, chip);
                float x = m_inner.min[0] + k_chip_pad;
                const float centre = m_inner.min[1] + header * 0.5f;
                for (uint32_t index = 0; index < shown; ++index)
                {
                    const float width = painter.text_width(m_entries[index].name, text_height);
                    painter.fill_rect({ { x, centre - swatch * 0.5f }, { swatch, swatch } }, m_entries[index].colour);
                    x += swatch + 3.0f;
                    painter.text({ { x, centre - text_height * 0.5f - 1.0f }, { width, text_height + 2.0f } }, m_entries[index].name, { text_height, muted, TextAlign::Left, false });
                    x += width + 8.0f;
                }
            }
        }
    }
    Memory next;
    next.valid = m_series > 0 || m_seen_count > 0;
    if (next.valid)
    {
        next.y_min = m_seen_min;
        next.y_max = m_seen_max;
        next.x_count = m_seen_count;
        next.series = m_series;
        next.bars = m_seen_bars;
    }
    else
    {
        next = m_previous;
    }
    m_context.state<Memory>(m_id) = next;
}

PlotArea PlotScope::area() const
{
    const bool known = m_previous.valid;
    const float seen_min = known ? m_previous.y_min : m_seen_min;
    const float seen_max = known ? m_previous.y_max : m_seen_max;
    const uint32_t count = known ? m_previous.x_count : m_seen_count;
    const bool bars = known ? m_previous.bars : m_seen_bars;
    PlotArea area;
    area.rect = m_data;
    area.x_min = 0.0f;
    area.x_max = count > 0 ? static_cast<float>(count) : 1.0f;
    resolve_y(seen_min, seen_max, bars, m_options.y_min, m_options.y_max, area.y_min, area.y_max);
    return area;
}

PlotResult PlotScope::result() const
{
    PlotResult result;
    result.item = m_canvas.item;
    const PlotArea plot = area();
    const bool has_data = (m_previous.valid ? m_previous.x_count : m_seen_count) > 0;
    if (m_canvas.item.hovered && has_data)
    {
        const float index = math::floor(index_at(plot, m_canvas.rect.min[0] + m_canvas.pointer_local[0]));
        result.hover_index = static_cast<uint32_t>(math::clamp(index, 0.0f, plot.x_max - 1.0f));
    }
    return result;
}

Colour PlotScope::series_colour(const Colour& requested) const
{
    return requested.a > 0.0f ? requested : palette_colour(m_context.gui_theme(), m_series);
}

void PlotScope::line(std::string_view label, const Values& values, PlotStyle style, const Colour& colour)
{
    if (m_series >= k_palette_size)
    {
        throw Error("PlotScope holds at most eight series", "draw fewer lines in one plot");
    }
    const Colour resolved = series_colour(colour);
    m_entries[m_series] = { m_context.arena().store(label), resolved };
    draw(values, style, false, resolved);
    ++m_series;
}

void PlotScope::bars(std::string_view label, const Values& values, const Colour& colour)
{
    if (m_series >= k_palette_size)
    {
        throw Error("PlotScope holds at most eight series", "draw fewer lines in one plot");
    }
    const Colour resolved = series_colour(colour);
    m_entries[m_series] = { m_context.arena().store(label), resolved };
    draw(values, PlotStyle::Area, true, resolved);
    ++m_series;
}

void PlotScope::vline(float index, const Colour& colour, float thickness)
{
    const PlotArea plot = area();
    m_canvas.painter.line(to_screen(plot, index + 0.5f, plot.y_max), to_screen(plot, index + 0.5f, plot.y_min), colour, thickness);
}

void PlotScope::hline(float value, const Colour& colour, float thickness)
{
    const PlotArea plot = area();
    m_canvas.painter.line(to_screen(plot, plot.x_min, value), to_screen(plot, plot.x_max, value), colour, thickness);
}

void PlotScope::draw(const Values& values, PlotStyle style, bool bars, const Colour& colour)
{
    const PlotArea plot = area();
    Painter& painter = m_canvas.painter;
    const float cell = plot.rect.size[0] / (plot.x_max - plot.x_min);
    const float width = plot.rect.size[0];
    const float baseline_value = math::clamp(0.0f, plot.y_min, plot.y_max);
    const float baseline = to_screen(plot, 0.0f, baseline_value)[1];
    Colour fill = colour;
    fill.a *= bars ? 1.0f : 0.35f;
    const bool filled = bars || style == PlotStyle::Area;
    const PlotStyle line_style = style == PlotStyle::Area ? PlotStyle::Line : style;

    Bucket bucket;
    Pen pen;
    const auto point_of = [&](uint32_t index, float value) { return to_screen(plot, static_cast<float>(index) + 0.5f, value); };
    const auto emit = [&](const Vec2f& point)
    {
        ++pen.emitted;
        if (line_style == PlotStyle::Points)
        {
            painter.fill_rect({ { point[0] - 1.5f, point[1] - 1.5f }, { 3.0f, 3.0f } }, colour);
        }
        else if (pen.down)
        {
            if (line_style == PlotStyle::Step)
            {
                painter.line(pen.last, { point[0], pen.last[1] }, colour, k_line_thickness);
                painter.line({ point[0], pen.last[1] }, point, colour, k_line_thickness);
            }
            else
            {
                painter.line(pen.last, point, colour, k_line_thickness);
            }
        }
        pen.last = point;
        pen.down = true;
    };
    const auto flush = [&]
    {
        if (bucket.count == 0)
        {
            return;
        }
        if (filled)
        {
            const float low = math::min(bucket.min, baseline_value);
            const float high = math::max(bucket.max, baseline_value);
            const float top = to_screen(plot, 0.0f, high)[1];
            const float bottom = to_screen(plot, 0.0f, low)[1];
            float left = plot.rect.min[0] + static_cast<float>(bucket.column);
            float span = 1.0f;
            if (cell >= 1.0f)
            {
                left = to_screen(plot, static_cast<float>(bucket.min_index), 0.0f)[0];
                span = cell;
                if (bars && cell >= k_min_bar_gap_cell)
                {
                    left += 1.0f;
                    span -= 2.0f;
                }
            }
            painter.fill_rect({ { left, top }, { span, bottom - top } }, fill);
        }
        if (!bars)
        {
            if (bucket.min_index == bucket.max_index)
            {
                emit(point_of(bucket.min_index, bucket.min));
            }
            else if (bucket.min_index < bucket.max_index)
            {
                emit(point_of(bucket.min_index, bucket.min));
                emit(point_of(bucket.max_index, bucket.max));
            }
            else
            {
                emit(point_of(bucket.max_index, bucket.max));
                emit(point_of(bucket.min_index, bucket.min));
            }
        }
        bucket.count = 0;
    };

    float seen_min = std::numeric_limits<float>::max();
    float seen_max = std::numeric_limits<float>::lowest();
    for (uint32_t index = 0; index < values.count; ++index)
    {
        const float value = value_at(values, index);
        if (!std::isfinite(value))
        {
            flush();
            pen.down = false;
            continue;
        }
        seen_min = math::min(seen_min, value);
        seen_max = math::max(seen_max, value);
        const float column_start = (static_cast<float>(index) - plot.x_min) * cell;
        if (column_start < -cell - 1.0f || column_start > width + 1.0f)
        {
            continue;
        }
        const int32_t column = static_cast<int32_t>(math::floor(column_start));
        if (bucket.count > 0 && column != bucket.column)
        {
            flush();
        }
        bucket.add(column, index, value);
    }
    flush();
    if (pen.emitted == 1 && line_style != PlotStyle::Points)
    {
        painter.fill_rect({ { pen.last[0] - 1.5f, pen.last[1] - 1.5f }, { 3.0f, 3.0f } }, colour);
    }
    m_seen_min = math::min(m_seen_min, seen_min);
    m_seen_max = math::max(m_seen_max, seen_max);
    m_seen_count = math::max(m_seen_count, values.count);
    m_seen_bars = m_seen_bars || bars;
}

PlotResult plot_lines(std::string_view name, const Values& values, const PlotOptions& options, PlotStyle style, const Colour& colour)
{
    PlotScope plot(name, options);
    plot.line(name, values, style, colour);
    return plot.result();
}

PlotResult plot_histogram(std::string_view name, const Values& values, const PlotOptions& options, const Colour& colour)
{
    PlotScope plot(name, options);
    plot.bars(name, values, colour);
    return plot.result();
}

PlotResult sparkline(std::string_view name, const Values& values, const SparklineOptions& options)
{
    PlotOptions plot_options;
    static_cast<WidgetOptions&>(plot_options) = options;
    plot_options.width = options.width;
    plot_options.height = options.height;
    plot_options.y_min = options.y_min;
    plot_options.y_max = options.y_max;
    plot_options.frame = false;
    plot_options.legend = false;
    plot_options.range_labels = false;
    return plot_lines(name, values, plot_options, PlotStyle::Line, options.colour);
}

ItemState heat_cell(std::string_view label, float value, const ColourScale& scale, const HeatCellOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
    const ItemState state = ctx.item(ctx.id(label));
    LayoutStyle box;
    if (options.layout != nullptr)
    {
        box = *options.layout;
    }
    else
    {
        box.width = options.width;
        box.height = options.height;
        box.align_x = Align::Centre;
        box.align_y = Align::Centre;
    }
    const Colour fill = evaluate(scale, value);
    const uint32_t index = ctx.begin_box(label, box);
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.has_fill = true;
    paint.fill = fill;
    paint.radius = uniform_radius(style.radius);
    if (state.hovered)
    {
        paint.border_width = style.border_width;
        paint.border = style.text;
    }
    if (options.show_value)
    {
        paint.text = format_number(options.format, value, ctx.arena());
        paint.text_height = style.text_height * 0.75f;
        paint.text_colour = contrasting(fill);
        paint.text_align = TextAlign::Centre;
    }
    ctx.end_box();
    return state;
}

HeatGridResult heat_grid(std::string_view name, const Values& values, uint32_t columns, const ColourScale& scale, const HeatGridOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = ctx.role_style(options, &GuiTheme::field);
    const uint32_t per_row = math::max(columns, 1u);
    const uint32_t rows = (values.count + per_row - 1) / per_row;
    const float stride_x = options.cell[0] + options.gap;
    const float stride_y = options.cell[1] + options.gap;
    const float width = static_cast<float>(per_row) * stride_x - options.gap;
    const float height = rows > 0 ? static_cast<float>(rows) * stride_y - options.gap : 0.0f;
    im::CanvasArea canvas = im::canvas(ctx, name, fixed(width), fixed(height));

    HeatGridResult result;
    if (canvas.item.hovered && stride_x > 0.0f && stride_y > 0.0f)
    {
        const float x = canvas.pointer_local[0];
        const float y = canvas.pointer_local[1];
        const float column = math::floor(x / stride_x);
        const float row = math::floor(y / stride_y);
        const bool inside = x >= 0.0f && y >= 0.0f && x - column * stride_x < options.cell[0] && y - row * stride_y < options.cell[1];
        if (inside && column < static_cast<float>(per_row))
        {
            const uint32_t index = static_cast<uint32_t>(row) * per_row + static_cast<uint32_t>(column);
            result.hovered_index = index < values.count ? index : k_no_index;
        }
    }
    if (canvas.item.clicked)
    {
        result.clicked_index = result.hovered_index;
    }
    for (uint32_t index = 0; index < values.count; ++index)
    {
        const float value = value_at(values, index);
        const Rect cell = { { canvas.rect.min[0] + static_cast<float>(index % per_row) * stride_x, canvas.rect.min[1] + static_cast<float>(index / per_row) * stride_y }, options.cell };
        const Colour fill = evaluate(scale, value);
        canvas.painter.fill_rect(cell, fill);
        if (options.show_values)
        {
            canvas.painter.text(cell, format_number(options.format, value, ctx.arena()), { style.text_height * 0.75f, contrasting(fill), TextAlign::Centre, true });
        }
        if (index == result.hovered_index)
        {
            canvas.painter.border(cell, {}, style.border_width, style.text);
        }
    }
    return result;
}

void legend_item(std::string_view name, const Colour& colour, const WidgetOptions& options)
{
    FieldOptions field;
    static_cast<WidgetOptions&>(field) = options;
    field.label_side = LabelSide::After;
    field.width = fit();
    colour_swatch(name, colour, field);
}

} // namespace oryx::gui
