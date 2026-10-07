#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

namespace
{

struct OpenFlag
{
    bool open = false;
};

struct DragAccumulator
{
    float value = 0.0f;
};

struct Track
{
    ImId id;
    ItemState item;
    ItemDrag drag;
    Rect rect;
    bool known = false;
};

// The bar a slider or drag value sits on: hit area, optional filled share (negative fraction for none) and the value drawn over it.
Track make_track(GuiContext& ctx, const ImStyle& style, float fraction, std::string_view text)
{
    Track track;
    track.id = ctx.id("track");
    track.item = ctx.item(track.id);
    track.drag = ctx.item_drag(track.id);
    track.known = ctx.layout_rect(track.id, track.rect);
    LayoutStyle box;
    box.width = grow(1.0f, 80.0f);
    box.height = fixed(style.text_height + 4.0f);
    const uint32_t index = ctx.begin_box(track.id, box);
    im::paint_surface(ctx.layout().node(index).paint, style, im::interaction_fill(style, track.item, style.background));
    if (fraction >= 0.0f)
    {
        LayoutStyle fill;
        fill.width = percent(math::saturate(fraction));
        fill.height = grow();
        const uint32_t fill_index = ctx.begin_box(ImId{}, fill);
        BoxPaint& paint = ctx.layout().node(fill_index).paint;
        paint.has_fill = true;
        paint.fill = style.accent;
        paint.radius = uniform_radius(style.radius);
        ctx.end_box();
    }
    overlay_text(text, style);
    ctx.end_box();
    return track;
}

float arrow_direction(const GuiContext& ctx, ImId focus)
{
    if (ctx.focus() != focus)
    {
        return 0.0f;
    }
    return (key_pressed(ctx.input().keys, ImKey::Right) ? 1.0f : 0.0f) - (key_pressed(ctx.input().keys, ImKey::Left) ? 1.0f : 0.0f);
}

std::string_view value_text(GuiContext& ctx, float value, bool integer)
{
    return integer ? ctx.arena().format("%d", static_cast<int>(std::lround(value))) : ctx.arena().format("%.2f", value);
}

bool slider_impl(std::string_view label, float& value, float min, float max, float step, bool integer, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    FieldScope field(label, options);
    const float range = max - min;
    const Track track = make_track(ctx, style, range > 0.0f ? (value - min) / range : 0.0f, value_text(ctx, value, integer));
    const float before = value;
    if (track.item.pressed)
    {
        ctx.set_focus(track.id);
    }
    if (track.item.held && track.known && track.rect.size[0] > 0.0f)
    {
        value = min + math::saturate((ctx.input().pointer.position[0] - track.rect.min[0]) / track.rect.size[0]) * range;
    }
    value += arrow_direction(ctx, track.id) * step;
    value = math::clamp(integer ? std::round(value) : value, min, math::max(min, max));
    return value != before;
}

bool drag_impl(std::string_view label, float& value, float min, float max, float speed, bool integer, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    FieldScope field(label, options);
    const Track track = make_track(ctx, style, -1.0f, value_text(ctx, value, integer));
    const float before = value;
    float& accumulated = ctx.state<DragAccumulator>(track.id).value;
    if (track.item.pressed)
    {
        ctx.set_focus(track.id);
        accumulated = value;
    }
    if (track.item.hovered || track.item.held)
    {
        ctx.request_cursor(CursorShape::ResizeHorizontal);
    }
    if (track.drag.started || track.drag.dragging)
    {
        accumulated += track.drag.delta[0] * speed;
        value = accumulated;
    }
    const float nudge = arrow_direction(ctx, track.id) * (integer ? math::max(1.0f, speed) : speed);
    value += nudge;
    accumulated += nudge;
    value = integer ? std::round(value) : value;
    if (min < max)
    {
        value = math::clamp(value, min, max);
        accumulated = math::clamp(accumulated, min, max);
    }
    return value != before;
}

} // namespace

bool slider_float(std::string_view label, float& value, float min, float max, const SliderOptions& options)
{
    return slider_impl(label, value, min, max, options.step > 0.0f ? options.step : (max - min) / 100.0f, false, options);
}

bool slider_int(std::string_view label, int32_t& value, int32_t min, int32_t max, const SliderOptions& options)
{
    float scratch = static_cast<float>(value);
    const bool changed = slider_impl(label, scratch, static_cast<float>(min), static_cast<float>(max), options.step > 0.0f ? options.step : 1.0f, true, options);
    value = static_cast<int32_t>(scratch);
    return changed;
}

bool drag_float(std::string_view label, float& value, float min, float max, const DragOptions& options)
{
    return drag_impl(label, value, min, max, options.speed, false, options);
}

bool drag_int(std::string_view label, int32_t& value, int32_t min, int32_t max, const DragOptions& options)
{
    float scratch = static_cast<float>(value);
    const bool changed = drag_impl(label, scratch, static_cast<float>(min), static_cast<float>(max), options.speed, true, options);
    value = static_cast<int32_t>(scratch);
    return changed;
}

bool combo(std::string_view label, std::span<const std::string_view> items, int32_t& selected, const FieldOptions& options)
{
    GuiContext& ctx = context();
    const ImStyle& style = im::resolved_style(ctx, options);
    FieldScope field(label, options);
    const ImId control = ctx.id("control");
    const ItemState state = ctx.item(control);
    bool open = ctx.state<OpenFlag>(control).open;
    if (state.clicked)
    {
        open = !open;
    }

    LayoutStyle button = im::default_box(style);
    button.width = grow(1.0f, 80.0f);
    button.align_x = Align::Start;
    const uint32_t index = ctx.begin_box(control, button);
    BoxPaint& paint = ctx.layout().node(index).paint;
    im::paint_surface(paint, style, im::interaction_fill(style, state, style.background));
    paint.text = selected >= 0 && selected < static_cast<int32_t>(items.size()) ? ctx.arena().store(items[static_cast<size_t>(selected)]) : std::string_view{};
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    paint.ellipsis = true;

    bool changed = false;
    if (open)
    {
        const ImId popup = ctx.id("popup");
        const PopupResult result = ctx.begin_popup_layer(popup);
        Rect anchor;
        Rect last;
        std::ignore = ctx.layout_rect(control, anchor);
        std::ignore = ctx.layout_rect(popup, last);
        LayoutStyle list;
        list.width = fit(anchor.size[0]);
        list.direction = Direction::Column;
        list.padding = uniform_insets(2.0f);
        list.channel = k_channel_popup;
        list.floating = im::popup_below(control, anchor, last.size, ctx.input().surface_size);
        const uint32_t list_index = ctx.begin_box(popup, list);
        im::paint_surface(ctx.layout().node(list_index).paint, style, style.background);
        ctx.push_id("popup");
        for (uint32_t item = 0; item < items.size(); ++item)
        {
            IdScope row(ctx, ctx.index_id(item));
            if (selectable(items[item], static_cast<int32_t>(item) == selected).clicked)
            {
                changed = selected != static_cast<int32_t>(item);
                selected = static_cast<int32_t>(item);
                open = false;
            }
        }
        ctx.pop_id();
        ctx.end_box();
        ctx.end_popup_layer();
        open = open && !result.closed_by_escape && !(result.closed_by_outside && !state.hovered);
    }
    ctx.end_box();
    ctx.state<OpenFlag>(control).open = open;
    return changed;
}

} // namespace oryx::gui
