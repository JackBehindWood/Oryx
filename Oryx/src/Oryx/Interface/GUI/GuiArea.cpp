#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiArea.h"

namespace oryx::gui
{

namespace
{

struct AreaState
{
    float size = -1.0f;
};

bool is_horizontal_edge(AreaAnchor anchor)
{
    return anchor == AreaAnchor::Top || anchor == AreaAnchor::Bottom;
}

float resolve_thickness(GuiContext& ctx, ImId id, const AreaOptions& options, float resized, float extent)
{
    const Sizing& size = options.size;
    float thickness = 0.0f;
    if (options.resizable && resized >= 0.0f)
    {
        thickness = resized;
    }
    else if (size.kind == SizingKind::Fixed)
    {
        thickness = size.value;
    }
    else if (size.kind == SizingKind::Percent)
    {
        thickness = size.value * extent;
    }
    else
    {
        Rect last;
        const bool known = ctx.layout_rect(id, last);
        thickness = known ? (is_horizontal_edge(options.anchor) ? last.size[1] : last.size[0]) : k_default_area_size;
    }
    return math::clamp(thickness, math::min(size.min, extent), math::min(size.max, extent));
}

Rect carve(AreaAnchor anchor, const Rect& remaining, float thickness, Rect& left_over)
{
    const Vec2f low = remaining.min;
    const Vec2f high = rect_max(remaining);
    Rect area = remaining;
    left_over = remaining;
    switch (anchor)
    {
        case AreaAnchor::Top:
            area.size[1] = thickness;
            left_over = { { low[0], low[1] + thickness }, { remaining.size[0], remaining.size[1] - thickness } };
            break;
        case AreaAnchor::Bottom:
            area = { { low[0], high[1] - thickness }, { remaining.size[0], thickness } };
            left_over.size[1] = remaining.size[1] - thickness;
            break;
        case AreaAnchor::Left:
            area.size[0] = thickness;
            left_over = { { low[0] + thickness, low[1] }, { remaining.size[0] - thickness, remaining.size[1] } };
            break;
        case AreaAnchor::Right:
            area = { { high[0] - thickness, low[1] }, { thickness, remaining.size[1] } };
            left_over.size[0] = remaining.size[0] - thickness;
            break;
        case AreaAnchor::Fill:
            break;
    }
    return area;
}

class AreaFrame
{
public:
    AreaFrame(std::string_view name, const AreaOptions& options, const Rect& rect, const Rect& space, bool measured, ImId id)
        : m_context(context())
        , m_options(options)
        , m_id(id)
        , m_extent(rect.size)
    {
        GuiContext& ctx = m_context;
        const ImStyle& style = ctx.role_style(options, &GuiTheme::panel);
        const Vec2f surface = ctx.input().surface_size;
        LayoutStyle box;
        box.width = fixed(rect.size[0]);
        box.height = fixed(rect.size[1]);
        box.direction = Direction::Column;
        box.overflow = Overflow::Clip;
        AttachPoint attach = AttachPoint::TopLeft;
        Vec2f offset = rect.min;
        if (measured)
        {
            const bool horizontal = is_horizontal_edge(options.anchor);
            Sizing& thickness = horizontal ? box.height : box.width;
            thickness = fit(options.size.min, options.size.max);
            if (options.anchor == AreaAnchor::Bottom)
            {
                attach = AttachPoint::BottomLeft;
                offset = { space.min[0], space.min[1] + space.size[1] - surface[1] };
            }
            else if (options.anchor == AreaAnchor::Right)
            {
                attach = AttachPoint::TopRight;
                offset = { space.min[0] + space.size[0] - surface[0], space.min[1] };
            }
        }
        box.floating = { true, attach, attach, FloatTarget::Root, {}, offset };
        const uint32_t index = ctx.begin_box(id, box);
        im::paint_surface(ctx.layout().node(index).paint, style, style.background);
        ctx.push_id(name);
        if (options.scroll)
        {
            begin_scroll("content");
        }
    }

    ~AreaFrame()
    {
        GuiContext& ctx = m_context;
        if (m_options.scroll)
        {
            end_scroll();
        }
        if (m_options.resizable && m_options.anchor != AreaAnchor::Fill)
        {
            grip();
        }
        ctx.pop_id();
        ctx.end_box();
    }

    AreaFrame(const AreaFrame&) = delete;
    AreaFrame& operator=(const AreaFrame&) = delete;

private:
    void grip()
    {
        GuiContext& ctx = m_context;
        const bool horizontal = is_horizontal_edge(m_options.anchor);
        const ImId grip_id = ctx.id("grip");
        const ItemState state = ctx.item(grip_id);
        const ItemDrag drag = ctx.item_drag(grip_id);
        if (state.hovered || state.held)
        {
            ctx.request_cursor(horizontal ? CursorShape::ResizeVertical : CursorShape::ResizeHorizontal);
        }
        if (drag.started || drag.dragging)
        {
            Rect last;
            if (ctx.layout_rect(m_id, last))
            {
                const float axis = horizontal ? drag.delta[1] : drag.delta[0];
                const bool grows_with_pointer = m_options.anchor == AreaAnchor::Top || m_options.anchor == AreaAnchor::Left;
                AreaState& state_of_size = ctx.state<AreaState>(m_id);
                const float current = state_of_size.size >= 0.0f ? state_of_size.size : horizontal ? last.size[1] : last.size[0];
                state_of_size.size = math::max(current + (grows_with_pointer ? axis : -axis), 0.0f);
            }
        }
        LayoutStyle bar;
        bar.width = horizontal ? fixed(m_extent[0]) : fixed(k_area_grip);
        bar.height = horizontal ? fixed(k_area_grip) : fixed(m_extent[1]);
        const AttachPoint edge = m_options.anchor == AreaAnchor::Top ? AttachPoint::BottomLeft : m_options.anchor == AreaAnchor::Bottom ? AttachPoint::TopLeft : m_options.anchor == AreaAnchor::Left ? AttachPoint::TopRight : AttachPoint::TopLeft;
        bar.floating = { true, edge, edge, FloatTarget::Parent, {}, { 0.0f, 0.0f } };
        const uint32_t index = ctx.begin_box(grip_id, bar);
        const ImStyle& style = ctx.gui_theme().panel;
        ctx.layout().node(index).paint.has_fill = state.hovered || state.held;
        ctx.layout().node(index).paint.fill = state.held ? style.accent : style.border;
        ctx.end_box();
    }

    GuiContext& m_context;
    AreaOptions m_options;
    ImId m_id;
    Vec2f m_extent;
};

} // namespace

bool begin_area(std::string_view name, const AreaOptions& options)
{
    GuiContext& ctx = context();
    if (options.open != nullptr && !*options.open)
    {
        return false;
    }
    const ImId id = ctx.id(name);
    const ItemState item = options.anchor == AreaAnchor::Fill ? ItemState{} : ctx.item(id);
    if (options.open != nullptr && options.mode == AreaMode::Overlay && item.hovered && key_pressed(ctx.input().keys, ImKey::Escape))
    {
        *options.open = false;
        return false;
    }
    Rect& remaining = ctx.area_remaining();
    const Rect space = remaining;
    const bool horizontal = is_horizontal_edge(options.anchor);
    const float extent = horizontal ? remaining.size[1] : remaining.size[0];
    const float resized = ctx.state<AreaState>(id).size;
    const float thickness = options.anchor == AreaAnchor::Fill ? extent : resolve_thickness(ctx, id, options, resized, extent);
    Rect left_over;
    const Rect rect = carve(options.anchor, remaining, thickness, left_over);
    if (options.mode == AreaMode::Reserve)
    {
        remaining = left_over;
    }
    const bool measured = options.anchor != AreaAnchor::Fill && !(options.resizable && resized >= 0.0f) && options.size.kind != SizingKind::Fixed && options.size.kind != SizingKind::Percent;
    ctx.push_scope<AreaFrame>("area", name, options, rect, space, measured, id);
    return true;
}

void end_area()
{
    context().pop_scope<AreaFrame>("end_area");
}

Rect area_rect(std::string_view name)
{
    GuiContext& ctx = context();
    Rect rect{ { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    std::ignore = ctx.layout_rect(ctx.id(name), rect);
    return rect;
}

Rect remaining_area()
{
    return context().area_remaining();
}

} // namespace oryx::gui
