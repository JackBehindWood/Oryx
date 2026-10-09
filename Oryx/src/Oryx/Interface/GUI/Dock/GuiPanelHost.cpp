#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/GuiPanelHost.h"
#include "Oryx/Interface/GUI/GuiControls.h"
#include "Oryx/Interface/GUI/GuiMenus.h"
#include "Oryx/Interface/GUI/GuiSettings.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

namespace
{

constexpr float k_landing_flash_seconds = 0.6f;

constexpr float k_close_size = 14.0f;

constexpr float k_splitter_pad = 3.0f;

DockView& host_of(GuiContext& ctx) { return ctx.dock_view(); }

bool host_active(const DockView& h, const GuiContext& ctx) { return h.in_host && h.frame == ctx.frame(); }

uint8_t without(uint8_t bits, bool clear, uint8_t flag) { return clear ? static_cast<uint8_t>(bits & ~flag) : bits; }

uint8_t flags_of(const PanelOptions& options)
{
    uint8_t bits = default_flags(options.kind).bits;
    bits = without(bits, options.no_dock, panel_flag::dock_elsewhere);
    bits = without(bits, options.no_close, panel_flag::close);
    bits = without(bits, options.no_collapse, panel_flag::collapse);
    bits = without(bits, options.no_resize, panel_flag::resize);
    bits = without(bits, options.no_reorder, panel_flag::reorder_in_host);
    bits = without(bits, options.no_tear_off, panel_flag::tear_off);
    return bits;
}

void report_unregistered(DockView& h, PanelId id)
{
    for (uint32_t i = 0; i < h.model->reported_count; ++i)
        if (h.model->reported[i] == id.hash)
            return;
    if (h.model->reported_count < k_max_reported_panels)
        h.model->reported[h.model->reported_count++] = id.hash;
    OX_WARN("Dock layout names an unregistered panel (hash {:08x}); it is skipped until registered", id.hash);
}

void note(DockView& h, const DockResult& result) { h.result.layout_changed = h.result.layout_changed || result.applied; }

// The last good layout is the only copy the host keeps; a bad one is swapped for it so the frame never throws.
void keep_valid(DockView& h, DockLayout& layout)
{
    if (h.model->has_good && equal(layout, h.model->last_good))
        return;
    try
    {
        validate(layout, h.model->panels, ValidateFlags{ h.require_viewport });
        h.model->last_good = layout;
        h.model->has_good = true;
    }
    catch (const Error& error)
    {
        OX_ERROR("Dock layout rejected, keeping the last good one: {}", error.what());
        layout = h.model->has_good ? h.model->last_good : DockLayout{};
        h.model->last_good = layout;
        h.model->has_good = true;
    }
}

void apply_history_request(DockView& h, DockLayout& layout)
{
    const HistoryRequest request = h.model->history_request;
    h.model->history_request = HistoryRequest::None;
    if (request == HistoryRequest::None || h.model->drag.source != DragSource::None)
        return;
    DockLayout snapshot;
    if (!(request == HistoryRequest::Undo ? undo(h.model->history, snapshot) : redo(h.model->history, snapshot)))
        return;
    layout = snapshot;
    keep_valid(h, layout);
    h.result.layout_changed = true;
}

DockMetrics metrics_of(const GuiTheme& theme, const DockStyle& style)
{
    const ImStyle& tab = theme.tab;
    const float scale = tab.text_height / 16.0f;
    const float density = style.compact ? 0.8f : 1.0f;
    DockMetrics metrics;
    metrics.style = style;
    metrics.strip_height = (tab.text_height + tab.padding.top + tab.padding.bottom) * density;
    metrics.toolbar_height = (theme.button.text_height + theme.button.padding.top + theme.button.padding.bottom + 4.0f * scale) * density;
    metrics.splitter = 4.0f * scale;
    metrics.tab_min_width = 48.0f * scale;
    metrics.tab_max_width = 160.0f * scale;
    metrics.guide_unit = tab.text_height;
    metrics.strip_button = metrics.strip_height;
    return metrics;
}

LayoutStyle placed(const DockView& h, const Rect& rect, uint32_t channel = 0)
{
    LayoutStyle style;
    style.channel = channel;
    style.width = fixed(rect.size[0]);
    style.height = fixed(rect.size[1]);
    style.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Element, h.host_id, { rect.min[0] - h.host_rect.min[0], rect.min[1] - h.host_rect.min[1] } };
    return style;
}

// Opens a filled floating box at `rect`; the caller closes it with end_box.
uint32_t open_fill(GuiContext& ctx, const DockView& h, const Rect& rect, const ImStyle& style, const Colour& fill, bool border, uint32_t channel = 0)
{
    const uint32_t index = ctx.begin_box(ImId{}, placed(h, rect, channel));
    BoxPaint& paint = ctx.layout().node(index).paint;
    im::paint_surface(paint, style, fill);
    if (!border)
        paint.border_width = 0.0f;
    return index;
}

int32_t find_body_index(const DockView& h, PanelId id)
{
    for (uint32_t i = 0; i < h.body_count; ++i)
        if (h.bodies[i].panel == id)
            return static_cast<int32_t>(i);
    return -1;
}

const PanelBody* find_body(const DockView& h, PanelId id)
{
    const int32_t index = find_body_index(h, id);
    return index < 0 ? nullptr : &h.bodies[index];
}

void add_body(DockView& h, PanelId id, const Rect& rect, const Rect& toolbar, uint32_t channel, bool viewport)
{
    if (h.body_count < k_max_panels)
        h.bodies[h.body_count++] = PanelBody{ id, rect, toolbar, channel, viewport };
}

// Records a panel's body and toolbar for begin_panel and draws their backdrops; true when the body took a press.
bool place_body(GuiContext& ctx, DockView& h, PanelId id, const Rect& body, const Rect& toolbar, uint32_t channel)
{
    const PanelDesc* desc = find_panel(h.model->panels, id);
    if (desc == nullptr)
    {
        report_unregistered(h, id);
        return false;
    }
    add_body(h, id, body, toolbar, channel, desc->kind == PanelKind::Viewport);
    if (!is_empty(toolbar))
    {
        const GuiTheme& theme = ctx.gui_theme();
        open_fill(ctx, h, toolbar, theme.header, theme.header.background, false, channel);
        ctx.end_box();
    }
    if (desc->kind == PanelKind::Viewport || is_empty(body))
        return false;
    const GuiTheme& theme = ctx.gui_theme();
    if (!desc->foreign_body)
    {
        open_fill(ctx, h, body, theme.panel, theme.panel.background, false, channel);
        if (h.model->focused == id)
        {
            BoxPaint& paint = ctx.layout().current().paint;
            paint.border_width = 1.0f;
            paint.border = ctx.dock_theme().focus_ring;
        }
        ctx.end_box();
    }
    IdScope scope(ctx, ctx.index_id(id.hash));
    return ctx.item(ctx.id("body"), body).pressed;
}

void draw_body(GuiContext& ctx, DockView& h, const DockNode& node, const SolvedNode& solved)
{
    if (solved.collapsed || node.selected >= node.count || is_empty(solved.body))
        return;
    const PanelId id = node.tabs[node.selected];
    if (place_body(ctx, h, id, solved.body, solved.toolbar, 0))
        h.model->focused = id;
}

void begin_drag(DockView& h, DragSource source, PanelId panel, const Vec2f& grab, const Rect& origin = {})
{
    if (h.model->drag.source != DragSource::None)
        return;
    h.model->drag = DragState{};
    h.model->drag.source = source;
    h.model->drag.panel = panel;
    h.model->drag.grab = grab;
    h.model->drag.origin_rect = origin;
}

// A panel pinned against reordering, docking and floating alike has nothing to drag.
bool draggable(const PanelTable& panels, PanelId panel)
{
    return can_reorder(panels, panel) == DockReason::None || can_dock(panels, panel) == DockReason::None || can_float(panels, panel) == DockReason::None;
}

Rect padded(const Rect& rect, float pad) { return Rect{ Vec2f(rect.min[0] - pad, rect.min[1] - pad), Vec2f(rect.size[0] + 2.0f * pad, rect.size[1] + 2.0f * pad) }; }

void close_mark(GuiContext& ctx, const ImStyle& style, const ItemState& close, float pad)
{
    LayoutStyle mark;
    mark.width = fixed(k_close_size);
    mark.height = fixed(k_close_size);
    mark.floating = { true, AttachPoint::CentreRight, AttachPoint::CentreRight, FloatTarget::Parent, {}, { -pad, 0.0f } };
    const uint32_t mark_index = ctx.begin_box(ImId{}, mark);
    BoxPaint& paint = ctx.layout().node(mark_index).paint;
    paint.has_fill = close.hovered;
    paint.fill = close.held ? style.pressed : style.hover;
    paint.radius = uniform_radius(style.radius);
    paint.icon = Icon::Cross;
    paint.icon_colour = close.hovered ? style.text : Colour{ style.text.r, style.text.g, style.text.b, style.text.a * 0.6f };
    paint.icon_size = 8.0f;
    ctx.end_box();
}

void draw_tab(GuiContext& ctx, DockView& h, int32_t node_index, const DockNode& node, uint32_t index, const Rect& rect, PanelId& pending_close)
{
    const PanelId id = node.tabs[index];
    const PanelDesc* desc = find_panel(h.model->panels, id);
    if (desc == nullptr)
    {
        report_unregistered(h, id);
        return;
    }
    const ImStyle& style = ctx.gui_theme().tab;
    const CloseButtons close_buttons = h.style.close_buttons;
    const bool chosen = index == node.selected;
    const bool closable = can_close(h.model->panels, id) == DockReason::None && close_buttons != CloseButtons::Never;
    const bool dragged = h.model->drag.source == DragSource::Tab && h.model->drag.panel == id;
    const float pad = style.padding.right * 0.5f;
    const Rect close_rect{ Vec2f(rect.min[0] + rect.size[0] - pad - k_close_size, rect.min[1]), Vec2f(k_close_size + pad, rect.size[1]) };

    IdScope scope(ctx, ctx.index_id(id.hash));
    const ImId tab_id = ctx.id("tab");
    const ItemState tab = ctx.item(tab_id, rect);
    const ItemDrag drag = ctx.item_drag(tab_id);
    const bool show_close = closable && (close_buttons == CloseButtons::Always || chosen || tab.hovered);
    ItemState close;
    if (show_close)
        close = ctx.item(ctx.id("close"), close_rect);
    if (tab.hovered)
        ctx.request_cursor(CursorShape::Hand);
    if (drag.started && !close.held && draggable(h.model->panels, id))
        begin_drag(h, DragSource::Tab, id, drag.start - rect.min);
    if (drag.started && !close.held && h.model->drag.panel == id)
        h.model->drag.origin_slot = index;

    LayoutStyle box = placed(h, rect);
    box.padding = style.padding;
    box.padding.right += show_close ? k_close_size + pad : 0.0f;
    box.align_y = Align::Centre;
    const uint32_t box_index = ctx.begin_box(ImId{}, box);
    {
        BoxPaint& paint = ctx.layout().node(box_index).paint;
        im::paint_surface(paint, style, im::interaction_fill(style, tab, chosen ? style.selected : style.background));
        paint.border_width = 0.0f;
        paint.text = ctx.arena().store(desc->title);
        paint.text_height = style.text_height;
        paint.text_colour = style.text;
        paint.ellipsis = true;
        if (dragged)
        {
            paint.fill.a *= k_disabled_alpha;
            paint.text_colour.a *= k_disabled_alpha;
            paint.border_width = 1.0f;
            paint.border = ctx.dock_theme().here_state;
        }
    }
    if (show_close)
        close_mark(ctx, style, close, pad);
    if (chosen)
    {
        LayoutStyle underline;
        underline.width = grow();
        underline.height = fixed(2.0f);
        const bool bottom = h.style.tab_position == TabPosition::Bottom;
        const AttachPoint edge = bottom ? AttachPoint::TopLeft : AttachPoint::BottomLeft;
        underline.floating = { true, edge, edge, FloatTarget::Parent, {}, { 0.0f, 0.0f } };
        const uint32_t line_index = ctx.begin_box(ImId{}, underline);
        BoxPaint& paint = ctx.layout().node(line_index).paint;
        paint.has_fill = true;
        paint.fill = style.accent;
        ctx.end_box();
    }
    ctx.end_box();
    if (h.model->drag.source == DragSource::None)
        tooltip(tab, desc->title);

    if (tab.pressed && !close.hovered)
    {
        h.model->focused = id;
        if (!chosen)
            note(h, select_tab(*h.layout, node_index, index));
    }
    if (close.clicked)
        pending_close = id;
}

void draw_strip(GuiContext& ctx, DockView& h, int32_t node_index, PanelId& pending_close)
{
    const DockNode& node = h.layout->nodes[node_index];
    const SolvedNode& solved = h.solved.nodes[node_index];
    const GuiTheme& theme = ctx.gui_theme();
    const ImStyle& style = theme.tab;
    const bool collapsed = solved.collapsed;

    if (solved.rail)
    {
        const float below = solved.strip.size[1];
        open_fill(ctx, h, Rect{ Vec2f(solved.rect.min[0], solved.rect.min[1] + below), Vec2f(solved.rect.size[0], solved.rect.size[1] - below) }, ctx.gui_theme().panel, ctx.gui_theme().panel.background, false);
        ctx.end_box();
    }
    open_fill(ctx, h, solved.strip, style, style.background, false);
    ctx.end_box();

    {
        IdScope scope(ctx, ctx.index_id(static_cast<uint64_t>(node_index)));
        const ItemState strip = ctx.item(ctx.id("strip"), solved.strip);
        const bool overflowing = solved.visible_count < node.count;
        if (strip.hovered && overflowing && !ctx.wheel_consumed())
        {
            const float wheel = ctx.input().wheel[1] != 0.0f ? ctx.input().wheel[1] : ctx.input().wheel[0];
            if (wheel != 0.0f)
            {
                std::ignore = ctx.consume_wheel();
                const uint32_t next = wheel > 0.0f ? (node.selected > 0 ? node.selected - 1u : 0u) : math::min<uint32_t>(node.selected + 1u, node.count - 1u);
                note(h, select_tab(*h.layout, node_index, next));
            }
        }
    }

    const uint32_t first = solved.first_visible;
    for (uint32_t i = first; i < first + solved.visible_count && i < node.count; ++i)
        draw_tab(ctx, h, node_index, node, i, solved.tab_rects[i], pending_close);

    if (!is_empty(solved.collapse_button))
    {
        IdScope scope(ctx, ctx.index_id(static_cast<uint64_t>(node_index)));
        const ItemState chevron = ctx.item(ctx.id("chevron"), solved.collapse_button);
        if (chevron.hovered)
            ctx.request_cursor(CursorShape::Hand);
        open_fill(ctx, h, solved.collapse_button, style, im::interaction_fill(style, chevron, style.background), false);
        BoxPaint& paint = ctx.layout().current().paint;
        paint.icon = collapsed ? Icon::ChevronRight : Icon::ChevronDown;
        paint.icon_colour = style.text;
        paint.icon_size = 8.0f;
        ctx.end_box();
        if (chevron.clicked)
            note(h, set_collapsed(*h.layout, h.model->panels, node_index, node.collapsed == 0));
    }
}

void draw_splitter(GuiContext& ctx, DockView& h, int32_t node_index)
{
    const DockNode& node = h.layout->nodes[node_index];
    const SolvedNode& solved = h.solved.nodes[node_index];
    const ImStyle& style = ctx.gui_theme().panel;
    ItemState state;
    if (can_resize_split(*h.layout, h.model->panels, node_index))
    {
        IdScope scope(ctx, ctx.index_id(static_cast<uint64_t>(node_index)));
        const ImId id = ctx.id("splitter");
        state = ctx.item(id, padded(solved.splitter, k_splitter_pad * ctx.gui_theme().tab.text_height / 16.0f));
        const ItemDrag drag = ctx.item_drag(id);
        const bool horizontal = node.axis == DockAxis::Horizontal;
        const uint32_t axis = horizontal ? 0 : 1;
        if (state.hovered || state.held)
            ctx.request_cursor(horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical);
        h.result.interacting = h.result.interacting || state.held;
        if (drag.started)
            h.model->anchor.size = h.solved.nodes[node.first].rect.size[axis];
        if (drag.started || drag.dragging)
        {
            const float first = h.model->anchor.size + drag.total[axis];
            const float total = solved.rect.size[axis] - solved.splitter.size[axis];
            if (total > 0.0f)
            {
                if (node.mode == DockSizeMode::Ratio)
                    note(h, set_split(*h.layout, h.model->panels, node_index, node.mode, first / total, node.points));
                else if (node.mode == DockSizeMode::FixedFirst)
                    note(h, set_split(*h.layout, h.model->panels, node_index, node.mode, node.ratio, first));
                else
                    note(h, set_split(*h.layout, h.model->panels, node_index, node.mode, node.ratio, total - first));
            }
        }
    }
    open_fill(ctx, h, solved.splitter, style, state.held ? style.accent : im::interaction_fill(style, state, style.border), false);
    ctx.end_box();
}

struct Grip
{
    const char* name;
    Rect rect;
    CursorShape cursor;
    bool width;
    bool height;
};

void draw_grips(GuiContext& ctx, DockView& h, uint32_t f)
{
    const Rect& rect = h.solved.floats[f];
    const float band = 6.0f;
    const float title = h.solved.float_parts[f].title.size[1];
    const float right = rect.min[0] + rect.size[0];
    const float bottom = rect.min[1] + rect.size[1];
    const Grip grips[3] = {
        { "grip_corner", Rect{ Vec2f(right - band * 2.0f, bottom - band * 2.0f), Vec2f(band * 2.0f, band * 2.0f) }, CursorShape::ResizeHorizontal, true, true },
        { "grip_right", Rect{ Vec2f(right - band * 0.5f, rect.min[1] + title), Vec2f(band, rect.size[1] - title - band * 2.0f) }, CursorShape::ResizeHorizontal, true, false },
        { "grip_bottom", Rect{ Vec2f(rect.min[0], bottom - band * 0.5f), Vec2f(rect.size[0] - band * 2.0f, band) }, CursorShape::ResizeVertical, false, true },
    };
    for (const Grip& grip : grips)
    {
        const ImId id = ctx.id(grip.name);
        const ItemState state = ctx.item(id, grip.rect);
        const ItemDrag drag = ctx.item_drag(id);
        if (state.hovered || state.held)
            ctx.request_cursor(grip.cursor);
        h.result.interacting = h.result.interacting || state.held;
        if (drag.started)
            h.model->anchor.rect = rect;
        if (drag.started || drag.dragging)
        {
            Rect resized = h.model->anchor.rect;
            resized.size = Vec2f(resized.size[0] + (grip.width ? drag.total[0] : 0.0f), resized.size[1] + (grip.height ? drag.total[1] : 0.0f));
            resized = clamp_float(resized, float_min_size(h.model->panels, h.layout->floats[f].panel, h.solved.metrics), h.solved.surface_rect);
            if (!(resized == h.layout->floats[f].rect))
            {
                h.layout->floats[f].rect = resized;
                h.result.layout_changed = true;
            }
        }
    }
}

void draw_float(GuiContext& ctx, DockView& h, uint32_t f, PanelId& pending_close, PanelId& pending_raise)
{
    const PanelId id = h.layout->floats[f].panel;
    h.float_rects[f] = h.solved.floats[f];
    h.float_panels[f] = id;
    h.float_count = f + 1;
    const PanelDesc* desc = find_panel(h.model->panels, id);
    if (desc == nullptr)
    {
        report_unregistered(h, id);
        return;
    }
    const GuiTheme& theme = ctx.gui_theme();
    const ImStyle& style = theme.tab;
    const Rect& rect = h.solved.floats[f];
    const SolvedFloat& parts = h.solved.float_parts[f];
    const bool focused = h.model->focused == id;
    const bool closable = can_close(h.model->panels, id) == DockReason::None;
    const float pad = style.padding.right * 0.5f;
    const Rect close_rect{ Vec2f(parts.title.min[0] + parts.title.size[0] - pad - k_close_size, parts.title.min[1]), Vec2f(k_close_size + pad, parts.title.size[1]) };

    IdScope scope(ctx, ctx.index_id(id.hash));
    ctx.add_shield(rect);
    ctx.begin_shield_layer();
    open_fill(ctx, h, rect, theme.panel, theme.panel.background, true, k_channel_popup);
    if (focused)
        ctx.layout().current().paint.border = ctx.dock_theme().focus_ring;
    ctx.end_box();

    const bool pressed_body = place_body(ctx, h, id, parts.body, parts.toolbar, k_channel_popup);
    if (pressed_body)
    {
        h.model->focused = id;
        pending_raise = id;
    }

    const ImId title_id = ctx.id("title");
    const ItemState title = ctx.item(title_id, parts.title);
    const ItemDrag drag = ctx.item_drag(title_id);
    ItemState close;
    if (closable)
        close = ctx.item(ctx.id("close"), close_rect);
    if (title.hovered)
        ctx.request_cursor(CursorShape::Hand);
    if (drag.started && !close.held)
        begin_drag(h, DragSource::Float, id, drag.start - rect.min, rect);

    LayoutStyle bar = placed(h, parts.title, k_channel_popup);
    bar.padding = style.padding;
    bar.padding.right += closable ? k_close_size + pad : 0.0f;
    bar.align_y = Align::Centre;
    const uint32_t bar_index = ctx.begin_box(ImId{}, bar);
    BoxPaint& paint = ctx.layout().node(bar_index).paint;
    im::paint_surface(paint, style, focused ? style.selected : style.background);
    paint.border_width = 0.0f;
    paint.text = ctx.arena().store(desc->title);
    paint.text_height = style.text_height;
    paint.text_colour = style.text;
    paint.ellipsis = true;
    if (closable)
        close_mark(ctx, style, close, pad);
    ctx.end_box();

    if (title.pressed && !close.hovered)
    {
        h.model->focused = id;
        pending_raise = id;
    }
    if (close.clicked)
        pending_close = id;
    draw_grips(ctx, h, f);
    ctx.end_shield_layer();
}

void raise_float(DockView& h, PanelId panel)
{
    DockLayout& layout = *h.layout;
    for (uint32_t f = 0; f + 1 < layout.float_count; ++f)
    {
        if (layout.floats[f].panel != panel)
            continue;
        const DockFloat raised = layout.floats[f];
        for (uint32_t i = f; i + 1 < layout.float_count; ++i)
            layout.floats[i] = layout.floats[i + 1];
        layout.floats[layout.float_count - 1] = raised;
        h.result.layout_changed = true;
        return;
    }
}

void overlay(GuiContext& ctx, const DockView& h, const Rect& rect, const Colour& fill, const Colour& border, float border_width)
{
    if (is_empty(rect))
        return;
    const uint32_t index = ctx.begin_box(ImId{}, placed(h, rect, k_channel_drag));
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.has_fill = fill.a > 0.0f;
    paint.fill = fill;
    paint.border_width = border_width;
    paint.border = border;
    ctx.end_box();
}

Rect target_rect(const DockView& h, const DropPlan& plan)
{
    if (plan.node == k_dock_root)
        return h.solved.surface_rect;
    if (plan.node >= 0 && static_cast<uint32_t>(plan.node) < h.solved.node_count)
        return h.solved.nodes[plan.node].rect;
    return {};
}

void note_label(GuiContext& ctx, const DockView& h, const Vec2f& at, const char* text)
{
    const ImStyle& note_style = ctx.gui_theme().overlay;
    // The label is laid out after this call, so its width is estimated to keep it inside the host.
    const float width = static_cast<float>(std::strlen(text)) * note_style.text_height * 0.55f + note_style.padding.left + note_style.padding.right;
    const float height = note_style.text_height + note_style.padding.top + note_style.padding.bottom;
    const Rect& host = h.host_rect;
    const Vec2f clamped(math::max(math::min(at[0], host.min[0] + host.size[0] - width), host.min[0]), math::max(math::min(at[1], host.min[1] + host.size[1] - height), host.min[1]));
    LayoutStyle label = placed(h, Rect{ clamped, Vec2f(0.0f, 0.0f) }, k_channel_drag);
    label.width = fit();
    label.height = fit();
    label.padding = note_style.padding;
    const uint32_t label_index = ctx.begin_box(ImId{}, label);
    BoxPaint& label_paint = ctx.layout().node(label_index).paint;
    im::paint_surface(label_paint, note_style, note_style.background);
    label_paint.text = ctx.arena().store(text);
    label_paint.text_height = note_style.text_height;
    label_paint.text_colour = note_style.text;
    ctx.end_box();
}

void reason_label(GuiContext& ctx, const DockView& h, const Vec2f& at, DockReason reason) { note_label(ctx, h, at, describe(reason)); }

Colour faded(Colour colour, float factor)
{
    colour.a *= factor;
    return colour;
}

Icon outward_chevron(DropZone zone)
{
    switch (zone)
    {
    case DropZone::Left: return Icon::ChevronLeft;
    case DropZone::Right: return Icon::ChevronRight;
    case DropZone::Top: return Icon::ChevronUp;
    case DropZone::Bottom: return Icon::ChevronDown;
    case DropZone::Centre: break;
    }
    return Icon::None;
}

void overlay_icon(GuiContext& ctx, const DockView& h, const Rect& rect, Icon icon, const Colour& colour)
{
    const uint32_t index = ctx.begin_box(ImId{}, placed(h, rect, k_channel_drag));
    BoxPaint& paint = ctx.layout().node(index).paint;
    paint.icon = icon;
    paint.icon_colour = colour;
    paint.icon_size = 10.0f;
    ctx.end_box();
}

// What the hovered guide does, in words: an inner guide works on one panel, an outer one on the whole window.
const char* guide_hint(const DropGuide& guide)
{
    if (guide.outer)
        return "Dock to the window edge";
    return guide.zone == DropZone::Centre ? "Add as a tab" : "Split this panel";
}

void draw_guides(GuiContext& ctx, const DockView& h, const DropGuides& guides)
{
    const GuiDockTheme& dock = ctx.dock_theme();
    const Colour clear{ 0.0f, 0.0f, 0.0f, 0.0f };
    const int32_t hovered = h.model->drag.plan.guide;
    if (guides.inner_node >= 0 && static_cast<uint32_t>(guides.inner_node) < h.solved.node_count)
        overlay(ctx, h, h.solved.nodes[guides.inner_node].rect, clear, faded(dock.preview_border, 0.35f), 1.0f);
    for (uint32_t i = 0; i < guides.count; ++i)
    {
        const DropGuide& guide = guides.guides[i];
        const bool over = static_cast<int32_t>(i) == hovered;
        const Rect box = over ? padded(guide.rect, 2.0f) : guide.rect;
        const Rect inset = padded(box, -2.0f);
        if (guide.here)
        {
            overlay(ctx, h, box, faded(dock.here_state, over ? 0.45f : 0.25f), clear, 0.0f);
            overlay(ctx, h, inset, clear, dock.here_state, 1.0f);
            continue;
        }
        const float alpha = guide.allowed ? 1.0f : k_disabled_alpha;
        const Colour& fill = guide.outer ? (over ? dock.outer_guide_hover : dock.outer_guide) : (over ? dock.guide_hover : dock.guide);
        const Colour glyph = faded(over ? dock.guide_glyph_hover : dock.guide_glyph, alpha);
        overlay(ctx, h, box, faded(fill, alpha), faded(guide.outer ? dock.outer_guide_hover : dock.guide_border, alpha), guide.outer ? 2.0f : 1.0f);
        if (guide.outer)
            overlay_icon(ctx, h, box, outward_chevron(guide.zone), glyph);
        else
            overlay(ctx, h, guide_glyph(box, guide.zone), glyph, clear, 0.0f);
    }
    if (hovered >= 0 && static_cast<uint32_t>(hovered) < guides.count)
    {
        const DropGuide& guide = guides.guides[hovered];
        const Vec2f below(guide.rect.min[0], guide.rect.min[1] + guide.rect.size[1] + 4.0f);
        if (!guide.allowed && !guide.here)
            reason_label(ctx, h, below, guide.reason);
        else if (guide.allowed)
            note_label(ctx, h, below, guide_hint(guide));
    }
}

const Rect* landed_rect(const DockView& h)
{
    for (uint32_t n = 0; n < h.solved.node_count; ++n)
    {
        const DockNode& node = h.layout->nodes[n];
        for (uint32_t t = 0; node.kind == DockNodeKind::Tabs && t < node.count; ++t)
            if (node.tabs[t] == h.model->landed)
                return &h.solved.nodes[n].rect;
    }
    for (uint32_t f = 0; f < h.solved.float_count; ++f)
        if (h.layout->floats[f].panel == h.model->landed)
            return &h.solved.floats[f];
    return nullptr;
}

// A fading accent frame on the panel that just docked, its tab stack or float.
void draw_landing_flash(GuiContext& ctx, DockView& h)
{
    // The drop that set it also reshaped the layout after this frame's solve, so the flash starts next frame.
    if (!is_valid(h.model->landed) || h.result.landed == h.model->landed)
        return;
    h.model->landed_age += ctx.delta_time();
    const float t = h.model->landed_age / k_landing_flash_seconds;
    const Rect* rect = landed_rect(h);
    if (t >= 1.0f || rect == nullptr || !h.style.drop_flash)
    {
        h.model->landed = PanelId{};
        return;
    }
    const Colour flash = ctx.dock_theme().landing_flash;
    const float fade = 1.0f - t;
    overlay(ctx, h, *rect, faded(flash, 0.22f * fade), faded(flash, fade), 2.0f);
}

void tab_chip(GuiContext& ctx, const DockView& h, const Rect& rect, std::string_view title, const Colour& border, float alpha)
{
    const ImStyle& style = ctx.gui_theme().tab;
    LayoutStyle chip = placed(h, rect, k_channel_drag);
    chip.padding = style.padding;
    chip.align_y = Align::Centre;
    const uint32_t index = ctx.begin_box(ImId{}, chip);
    BoxPaint& paint = ctx.layout().node(index).paint;
    im::paint_surface(paint, style, faded(style.selected, alpha));
    paint.border_width = 1.0f;
    paint.border = border;
    paint.text = ctx.arena().store(title);
    paint.text_height = style.text_height;
    paint.text_colour = faded(style.text, alpha);
    paint.ellipsis = true;
    ctx.end_box();
}

void draw_drag_feedback(GuiContext& ctx, DockView& h)
{
    const DragState& d = h.model->drag;
    const DropPlan& plan = d.plan;
    const GuiDockTheme& dock = ctx.dock_theme();
    const bool beside_tree = plan.action == DropAction::Dock && plan.node == k_dock_root;
    Colour preview = beside_tree ? faded(dock.outer_guide_hover, 0.28f) : dock.preview;
    preview.a *= h.style.preview_opacity;
    overlay(ctx, h, plan.preview, preview, beside_tree ? dock.outer_guide_hover : dock.preview_border, 1.0f);
    overlay(ctx, h, plan.marker, dock.preview_border, dock.preview_border, 0.0f);
    if (plan.action == DropAction::Float)
    {
        const Rect title{ plan.preview.min, Vec2f(plan.preview.size[0], h.solved.metrics.strip_height) };
        overlay(ctx, h, title, faded(dock.preview_border, 0.45f), dock.preview_border, 1.0f);
    }
    else if (!ctx.input().keys.shift)
        draw_guides(ctx, h, d.guides);

    const bool refused = plan.action == DropAction::Cancel && plan.reason != DockReason::TargetInvalid && plan.reason != DockReason::NotFound;
    if (refused)
    {
        const Colour danger = dock.refusal;
        overlay(ctx, h, target_rect(h, plan), Colour{ danger.r, danger.g, danger.b, 0.12f }, danger, 1.0f);
    }
    if (d.source != DragSource::Tab || plan.action == DropAction::Float)
        return;

    const PanelDesc* desc = find_panel(h.model->panels, d.panel);
    const std::string_view title = desc != nullptr ? std::string_view(desc->title) : std::string_view();
    const float chip_w = h.solved.metrics.tab_max_width;
    const float chip_h = h.solved.metrics.strip_height;
    if (plan.action == DropAction::Dock && plan.zone == DropZone::Centre && plan.node >= 0 && !is_empty(plan.marker))
        tab_chip(ctx, h, Rect{ Vec2f(plan.marker.min[0], h.solved.nodes[plan.node].strip.min[1]), Vec2f(chip_w, chip_h) }, title, dock.preview_border, 0.6f);
    Vec2f at = ctx.input().pointer.position - d.grab;
    if (plan.action == DropAction::None && plan.node != k_no_node && static_cast<uint32_t>(plan.node) < h.solved.node_count)
        at[1] = h.solved.nodes[plan.node].strip.min[1];
    const Rect ghost{ at, Vec2f(chip_w, chip_h) };
    tab_chip(ctx, h, ghost, title, refused ? dock.refusal : dock.chip_border, 1.0f);
    if (refused && plan.guide < 0)
        reason_label(ctx, h, Vec2f(at[0], at[1] + chip_h + 2.0f), plan.reason);
}

void apply_drop(DockView& h, const DragState& d)
{
    DockLayout& layout = *h.layout;
    const DropPlan& plan = d.plan;
    const DockLayout before = layout;
    if (plan.action == DropAction::Reorder)
    {
        note(h, reorder_tab(layout, h.model->panels, d.panel, plan.slot));
    }
    else if (plan.action == DropAction::Dock)
    {
        const DockResult docked = dock_panel(layout, h.model->panels, d.panel, DockTarget{ plan.node, plan.surface }, plan.zone);
        note(h, docked);
        if (docked.applied && plan.zone == DropZone::Centre)
            note(h, reorder_tab(layout, h.model->panels, d.panel, plan.slot));
    }
    else if (plan.action == DropAction::Float)
    {
        note(h, float_panel(layout, h.model->panels, d.panel, plan.preview, plan.surface));
    }
    if (!equal(before, layout))
    {
        h.model->landed = d.panel;
        h.model->landed_age = 0.0f;
        h.result.landed = d.panel;
    }
}

void move_dragged_float(DockView& h, const Vec2f& pointer)
{
    DockLayout& layout = *h.layout;
    const Rect& surface = h.solved.surface_rect;
    for (uint32_t f = 0; f < layout.float_count; ++f)
    {
        if (layout.floats[f].panel != h.model->drag.panel)
            continue;
        const Rect& size = h.solved.floats[f];
        const Vec2f at(math::clamp(pointer[0] - h.model->drag.grab[0], surface.min[0], surface.min[0] + surface.size[0] - size.size[0]), math::clamp(pointer[1] - h.model->drag.grab[1], surface.min[1], surface.min[1] + surface.size[1] - size.size[1]));
        if (at[0] != layout.floats[f].rect.min[0] || at[1] != layout.floats[f].rect.min[1] || size.size[0] != layout.floats[f].rect.size[0] || size.size[1] != layout.floats[f].rect.size[1])
        {
            layout.floats[f].rect = Rect{ at, size.size };
            h.result.layout_changed = true;
        }
        return;
    }
}

// Moves the dragged tab to where the pointer is: it passes a neighbour once the pointer crosses that neighbour's centre, so a swap never flips back under the same pointer.
void reorder_live(DockView& h, DragState& d, const Vec2f& pointer)
{
    const DockNode& node = h.layout->nodes[d.plan.node];
    const SolvedNode& solved = h.solved.nodes[d.plan.node];
    uint32_t current = 0;
    while (current < node.count && node.tabs[current] != d.panel)
        ++current;
    const uint32_t first = solved.first_visible;
    const uint32_t last = math::min<uint32_t>(first + solved.visible_count, node.count) - 1u;
    if (solved.visible_count == 0 || current < first || current > last)
        return;
    const auto centre = [&solved](uint32_t i) { return solved.tab_rects[i].min[0] + solved.tab_rects[i].size[0] * 0.5f; };
    uint32_t target = current;
    while (target > first && pointer[0] < centre(target - 1u))
        --target;
    while (target < last && pointer[0] > centre(target + 1u))
        ++target;
    if (target != current && reorder_tab(*h.layout, h.model->panels, d.panel, target).applied)
    {
        d.reordered = true;
        h.result.layout_changed = true;
    }
}

void restore_reorder(DockView& h, DragState& d)
{
    if (d.reordered && reorder_tab(*h.layout, h.model->panels, d.panel, d.origin_slot).applied)
        h.result.layout_changed = true;
}

void restore_float(DockView& h, const DragState& d)
{
    DockLayout& layout = *h.layout;
    for (uint32_t f = 0; f < layout.float_count; ++f)
        if (layout.floats[f].panel == d.panel && !(layout.floats[f].rect == d.origin_rect) && d.origin_rect.size[0] > 0.0f)
        {
            layout.floats[f].rect = d.origin_rect;
            h.result.layout_changed = true;
        }
}

// Runs after the frame's drawing so no op reshapes the tree while its tabs are still being drawn.
void update_drag(GuiContext& ctx, DockView& h)
{
    DragState& d = h.model->drag;
    if (d.source == DragSource::None)
        return;
    const ImInput& input = ctx.input();
    if (key_pressed(input.keys, ImKey::Escape) || !is_open(*h.layout, d.panel))
    {
        if (key_pressed(input.keys, ImKey::Escape))
        {
            restore_reorder(h, d);
            if (d.source == DragSource::Float)
                restore_float(h, d);
        }
        d = DragState{};
        return;
    }
    const Vec2f pointer = input.pointer.position;
    if (d.source == DragSource::Float)
        move_dragged_float(h, pointer);
    d.guides = input.keys.shift ? DropGuides{} : drop_guides(*h.layout, h.model->panels, h.solved, d.panel, pointer);
    d.plan = resolve_drop(*h.layout, h.model->panels, h.solved, d.guides, d.panel, pointer, input.keys.shift);
    if (d.source == DragSource::Tab && d.plan.action == DropAction::Reorder)
    {
        reorder_live(h, d, pointer);
        d.plan = DropPlan{ DropAction::None, d.plan.node };
    }
    if (!button_of(input, MouseCode::Left).down)
    {
        if (d.plan.action == DropAction::Cancel)
            restore_reorder(h, d);
        apply_drop(h, d);
        d = DragState{};
        return;
    }
    h.result.interacting = true;
    h.result.dragging = true;
    const bool accepted = d.plan.action == DropAction::Dock || d.plan.action == DropAction::Float || d.plan.action == DropAction::Reorder;
    const bool in_own_strip = d.source == DragSource::Tab && d.plan.action == DropAction::None && d.plan.node != k_no_node;
    ctx.request_cursor(accepted || in_own_strip ? CursorShape::Hand : CursorShape::Arrow);
    draw_drag_feedback(ctx, h);
}

// Ctrl+Tab walks the tabs of the focused panel's node; a node with one tab hands the walk to the next node.
void cycle_tabs(DockView& h, const ImKeys& keys)
{
    if (!keys.ctrl || !key_pressed(keys, ImKey::Tab) || h.model->drag.source != DragSource::None)
        return;
    DockLayout& layout = *h.layout;
    const int32_t step = keys.shift ? -1 : 1;
    int32_t current = k_no_node;
    for (uint32_t n = 0; n < layout.node_count && current == k_no_node; ++n)
    {
        const DockNode& node = layout.nodes[n];
        for (uint32_t t = 0; node.kind == DockNodeKind::Tabs && t < node.count; ++t)
            if (node.tabs[t] == h.model->focused)
                current = static_cast<int32_t>(n);
    }
    if (current == k_no_node)
    {
        for (uint32_t n = 0; n < layout.node_count && current == k_no_node; ++n)
            if (layout.nodes[n].kind == DockNodeKind::Tabs && layout.nodes[n].count > 0)
                current = static_cast<int32_t>(n);
        if (current == k_no_node)
            return;
        h.model->focused = layout.nodes[current].tabs[layout.nodes[current].selected];
        return;
    }
    const DockNode& node = layout.nodes[current];
    int32_t target = current;
    uint32_t index = 0;
    if (node.count > 1)
    {
        index = static_cast<uint32_t>((static_cast<int32_t>(node.selected) + step + static_cast<int32_t>(node.count)) % static_cast<int32_t>(node.count));
    }
    else
    {
        const int32_t total = static_cast<int32_t>(layout.node_count);
        for (int32_t i = 1; i <= total; ++i)
        {
            const int32_t candidate = ((current + step * i) % total + total) % total;
            if (layout.nodes[candidate].kind == DockNodeKind::Tabs && layout.nodes[candidate].count > 0)
            {
                target = candidate;
                break;
            }
        }
        index = layout.nodes[target].selected;
    }
    note(h, select_tab(layout, target, index));
    h.model->focused = layout.nodes[target].tabs[index];
    h.result.layout_changed = true;
}

void draw_empty(GuiContext& ctx, DockView& h)
{
    LayoutStyle box;
    box.direction = Direction::Column;
    box.align_x = Align::Centre;
    box.gap = spacing();
    box.floating = { true, AttachPoint::Centre, AttachPoint::Centre, FloatTarget::Element, h.host_id, {} };
    ctx.begin_box("panel_host_empty", box);
    ctx.push_id("panel_host_empty");
    label("No panels open");
    if (button("Reset layout").clicked)
    {
        *h.layout = h.model->default_layout;
        h.result.layout_changed = true;
    }
    ctx.pop_id();
    ctx.end_box();
}

void draw_host(GuiContext& ctx, DockView& h)
{
    h.solved = solve(*h.layout, h.model->panels, metrics_of(ctx.gui_theme(), h.style), h.host_rect, static_cast<uint8_t>(ctx.input().surface));
    const int32_t root = h.layout->roots[ctx.input().surface];
    if (root == k_no_node && h.layout->float_count == 0)
    {
        draw_empty(ctx, h);
        return;
    }
    ctx.push_id("panel_host");
    cycle_tabs(h, ctx.input().keys);
    PanelId pending_close;
    PanelId pending_raise;
    const uint32_t count = h.solved.node_count;
    for (uint32_t n = 0; n < count; ++n)
    {
        const DockNode& node = h.layout->nodes[n];
        if (node.kind == DockNodeKind::Tabs)
        {
            ctx.push_id("bodies");
            draw_body(ctx, h, node, h.solved.nodes[n]);
            ctx.pop_id();
        }
    }
    for (uint32_t n = 0; n < count; ++n)
    {
        const DockNode& node = h.layout->nodes[n];
        if (node.kind == DockNodeKind::Tabs)
        {
            ctx.push_id("strips");
            draw_strip(ctx, h, static_cast<int32_t>(n), pending_close);
            ctx.pop_id();
        }
        else if (node.kind == DockNodeKind::Split)
        {
            ctx.push_id("splits");
            draw_splitter(ctx, h, static_cast<int32_t>(n));
            ctx.pop_id();
        }
    }
    ctx.push_id("floats");
    for (uint32_t f = 0; f < h.solved.float_count; ++f)
        if (h.layout->floats[f].surface == h.solved.surface)
            draw_float(ctx, h, f, pending_close, pending_raise);
    ctx.pop_id();
    ctx.pop_id();
    update_drag(ctx, h);
    draw_landing_flash(ctx, h);
    if (is_valid(pending_raise))
        raise_float(h, pending_raise);
    if (is_valid(pending_close))
        note(h, close_panel(*h.layout, h.model->panels, pending_close));
}


bool has_viewport(const PanelTable& panels)
{
    for (uint32_t i = 0; i < panels.count; ++i)
        if (panels.descs[i].kind == PanelKind::Viewport)
            return true;
    return false;
}

bool viewport_open(const DockLayout& layout, const PanelTable& panels)
{
    for (uint32_t i = 0; i < panels.count; ++i)
        if (panels.descs[i].kind == PanelKind::Viewport && is_open(layout, panels.descs[i].id))
            return true;
    return false;
}

// Loads the layout on first use: the file when docking is on and a layout_file is set, else the in-memory default. Registration must be done by then.
void ensure_loaded(DockModel& m)
{
    const GuiSettings& settings = settings_of<GuiSettings>();
    const bool use_file = settings.docking && !settings.layout_file.empty();
    if (m.load == DockLoad::File || (m.load == DockLoad::DefaultOnly && !use_file))
        return;
    if (!m.default_set)
        m.default_layout = build_default_layout(m.panels);
    DockFileLoad loaded;
    loaded.layout = m.default_layout;
    if (use_file)
        loaded = load_layout_file(settings.layout_file, m.panels, m.default_layout);
    migrate_surfaces(loaded.layout, m.panels, m.surface_count);
    const bool usable = !has_viewport(m.panels) || viewport_open(loaded.layout, m.panels);
    m.layout = usable ? loaded.layout : m.default_layout;
    m.from_file = loaded.from_file && usable;
    m.has_good = false;
    m.file = use_file ? settings.layout_file : std::filesystem::path();
    m.autosave = use_file ? create_unique<DockAutosave>(m.layout, loaded.autosave_allowed) : nullptr;
    m.load = use_file ? DockLoad::File : DockLoad::DefaultOnly;
    reset(m.history, m.layout);
    m.history_seeded = true;
}

DockModel& loaded_model()
{
    DockModel& m = context().dock_model();
    ensure_loaded(m);
    return m;
}

bool tab_location(const DockLayout& layout, PanelId panel, int32_t& node, uint32_t& index)
{
    for (uint32_t n = 0; n < layout.node_count && n < k_max_dock_nodes; ++n)
    {
        const DockNode& candidate = layout.nodes[n];
        for (uint32_t t = 0; candidate.kind == DockNodeKind::Tabs && t < candidate.count && t < k_max_dock_tabs; ++t)
            if (candidate.tabs[t] == panel)
            {
                node = static_cast<int32_t>(n);
                index = t;
                return true;
            }
    }
    return false;
}

DockResult unknown_panel() { return DockResult{ false, DockReason::UnknownPanel }; }

// The panels' viewport body takes the whole host while docking is off.
void draw_flat_host(DockView& h)
{
    const PanelTable& table = h.model->panels;
    for (uint32_t i = 0; i < table.count; ++i)
        if (table.descs[i].kind == PanelKind::Viewport)
        {
            add_body(h, table.descs[i].id, h.host_rect, Rect{}, 0, true);
            return;
        }
}

// A press inside a viewport body that no widget took focuses it.
void focus_pressed_viewport(GuiContext& ctx, DockView& h)
{
    const ImInput& input = ctx.input();
    if (!input.pointer.buttons[0].pressed || ctx.wants_mouse() || ctx.popup_open())
        return;
    for (uint32_t i = 0; i < h.body_count; ++i)
        if (h.bodies[i].viewport && contains(h.bodies[i].rect, input.pointer.position))
        {
            h.model->focused = h.bodies[i].panel;
            return;
        }
}

}

DockModel::~DockModel()
{
    try
    {
        if (autosave != nullptr && load == DockLoad::File)
            autosave->flush(layout, file, panels);
    }
    catch (...)
    {
    }
}

bool register_panel(std::string_view name, const PanelOptions& options)
{
    DockModel& m = context().dock_model();
    if (!add_panel(m.panels, name, options.title.empty() ? name : options.title, options.kind, options.min_w, options.min_h))
        return false;
    PanelDesc* desc = find_panel(m.panels, make_panel_id(name));
    desc->flags = PanelFlags{ flags_of(options) };
    desc->toolbar = options.toolbar;
    desc->toolbar_placement = options.toolbar_placement;
    desc->foreign_body = options.foreign_body;
    desc->dock_near = options.dock_near.empty() ? PanelId{} : make_panel_id(options.dock_near);
    desc->dock_tabbed_with = options.dock_tabbed_with.empty() ? PanelId{} : make_panel_id(options.dock_tabbed_with);
    desc->group = options.group.empty() ? PanelId{} : make_panel_id(options.group);
    desc->dock_side = options.dock_side;
    desc->dock_size = options.dock_size;
    desc->order = options.order;
    desc->initial_open = options.initial_open;
    return true;
}

const PanelTable& panels()
{
    return context().dock_model().panels;
}

bool dock_only_in(std::string_view panel, std::string_view target)
{
    return add_dock_only(context().dock_model().panels, make_panel_id(panel), make_panel_id(target));
}

bool never_dock_in(std::string_view panel, std::string_view target)
{
    return add_dock_never(context().dock_model().panels, make_panel_id(panel), make_panel_id(target));
}

void begin_panel_host()
{
    GuiContext& ctx = context();
    DockView& h = host_of(ctx);
    DockModel& m = *h.model;
    if (host_active(h, ctx))
        throw Error("begin_panel_host called inside a panel host", "call end_panel_host first");
    ensure_loaded(m);
    const GuiSettings& settings = settings_of<GuiSettings>();
    h.in_host = true;
    h.frame = ctx.frame();
    h.dock_body_open = false;
    h.depth = 0;
    h.layout = &m.layout;
    h.require_viewport = has_viewport(m.panels);
    h.style = settings.dock;
    h.result = {};
    h.body_count = 0;
    h.current_body = -1;
    h.toolbar_open = false;
    h.float_count = 0;
    h.solved.node_count = 0;
    if (settings.docking)
    {
        keep_valid(h, m.layout);
        apply_history_request(h, m.layout);
    }
    else
    {
        m.drag = DragState{};
        m.history_request = HistoryRequest::None;
    }

    h.host_id = ctx.id("panel_host");
    LayoutStyle area;
    area.width = grow();
    area.height = grow();
    ctx.begin_box(h.host_id, area);
    ctx.end_box();
    const bool known = ctx.layout_rect(h.host_id, h.host_rect);
    if (!known)
        return;
    if (!settings.docking)
        draw_flat_host(h);
    else if (ctx.input().surface < k_max_dock_surfaces)
        draw_host(ctx, h);
}

void end_panel_host()
{
    GuiContext& ctx = context();
    DockView& h = host_of(ctx);
    DockModel& m = *h.model;
    if (!host_active(h, ctx))
        throw Error("end_panel_host without begin_panel_host", "every end_panel_host needs a begin_panel_host");
    if (h.depth != 0)
        throw Error("end_panel_host with a panel still open", "every begin_panel needs an end_panel");
    if (h.toolbar_open)
        throw Error("end_panel_host with a toolbar still open", "every begin_panel_toolbar needs an end_panel_toolbar");
    const GuiSettings& settings = settings_of<GuiSettings>();
    focus_pressed_viewport(ctx, h);
    if (settings.docking)
    {
        if (h.result.layout_changed)
            keep_valid(h, m.layout);
        if (is_valid(m.focused) && !is_open(m.layout, m.focused))
            m.focused = PanelId{};
        if (!h.result.interacting)
            push(m.history, m.layout);
        if (m.autosave != nullptr && !settings.layout_file.empty())
            static_cast<void>(m.autosave->update(m.layout, h.result.interacting || m.drag.source != DragSource::None, settings.layout_file, m.panels));
    }
    h.in_host = false;
}

bool begin_panel(std::string_view name, const PanelOptions& options)
{
    GuiContext& ctx = context();
    DockView& h = host_of(ctx);
    if (h.depth >= k_max_panel_stack)
        throw Error("begin_panel nests too deep", "close panels with end_panel");
    if (!host_active(h, ctx) || h.dock_body_open)
    {
        WidgetOptions resolved = options;
        resolved.style = &ctx.role_style(options, &GuiTheme::panel);
        im::begin_panel(ctx, name, resolved);
        h.stack[h.depth++] = PanelCall::Plain;
        return true;
    }

    const PanelId id = make_panel_id(name);
    if (find_panel(h.model->panels, id) == nullptr)
        std::ignore = register_panel(name, options);
    const int32_t body_index = find_body_index(h, id);
    const PanelBody* body = body_index < 0 ? nullptr : &h.bodies[body_index];
    if (body == nullptr || body->viewport)
    {
        h.stack[h.depth++] = PanelCall::DockHidden;
        return false;
    }
    LayoutStyle box = im::widget_box(ctx.role_style(options, &GuiTheme::panel), options);
    box.direction = Direction::Column;
    box.align_x = Align::Start;
    box.align_y = Align::Start;
    box.overflow = Overflow::Clip;
    const LayoutStyle frame = placed(h, body->rect, body->channel);
    box.width = frame.width;
    box.height = frame.height;
    box.floating = frame.floating;
    box.channel = frame.channel;
    const bool floating = body->channel != 0;
    if (floating)
        ctx.begin_shield_layer();
    ctx.begin_box(name, box);
    ctx.push_id(name);
    begin_scroll("content");
    h.dock_body_open = true;
    h.current_body = body_index;
    h.stack[h.depth++] = floating ? PanelCall::DockFloat : PanelCall::Dock;
    return true;
}

void end_panel()
{
    GuiContext& ctx = context();
    DockView& h = host_of(ctx);
    if (h.depth == 0)
        throw Error("end_panel without begin_panel", "every end_panel needs a begin_panel");
    const PanelCall call = h.stack[--h.depth];
    if (call == PanelCall::Plain)
    {
        im::end_panel(ctx);
    }
    else if (call == PanelCall::Dock || call == PanelCall::DockFloat)
    {
        if (h.toolbar_open)
            throw Error("end_panel with a toolbar still open", "every begin_panel_toolbar needs an end_panel_toolbar");
        h.current_body = -1;
        end_scroll();
        ctx.pop_id();
        ctx.end_box();
        if (call == PanelCall::DockFloat)
            ctx.end_shield_layer();
        h.dock_body_open = false;
    }
}

Rect viewport_rect(std::string_view name)
{
    const PanelBody* body = find_body(host_of(context()), make_panel_id(name));
    return body != nullptr && body->viewport ? body->rect : Rect{};
}

Rect panel_rect(std::string_view name)
{
    const PanelBody* body = find_body(host_of(context()), make_panel_id(name));
    return body != nullptr ? body->rect : Rect{};
}

bool panel_occluded(std::string_view name, const Vec2f& point)
{
    const DockView& h = host_of(context());
    const PanelId id = make_panel_id(name);
    uint32_t above = 0;
    for (uint32_t f = 0; f < h.float_count; ++f)
        if (h.float_panels[f] == id)
            above = f + 1;
    for (uint32_t f = above; f < h.float_count; ++f)
        if (contains(h.float_rects[f], point))
            return true;
    return false;
}

bool panel_blocked(std::string_view name)
{
    GuiContext& ctx = context();
    return panel_occluded(name, ctx.input().pointer.position) || ctx.popup_open();
}

bool panel_claims_pointer(std::string_view name, const GuiContext& other)
{
    return !is_empty(panel_rect(name)) && (other.wants_mouse() || other.popup_open());
}

bool panel_claims_keyboard(std::string_view name, const GuiContext& other)
{
    return !is_empty(panel_rect(name)) && (other.wants_keyboard() || other.popup_open());
}

PanelHostResult panel_host_result()
{
    return host_of(context()).result;
}

bool begin_panel_toolbar()
{
    GuiContext& ctx = context();
    DockView& h = host_of(ctx);
    if (!host_active(h, ctx) || !h.dock_body_open || h.current_body < 0)
        return false;
    if (h.toolbar_open)
        throw Error("begin_panel_toolbar called twice", "close the toolbar with end_panel_toolbar");
    const PanelBody& body = h.bodies[h.current_body];
    if (is_empty(body.toolbar))
        return false;
    LayoutStyle box = placed(h, body.toolbar, body.channel);
    box.direction = Direction::Row;
    box.align_y = Align::Centre;
    box.gap = ctx.gui_theme().spacing;
    box.padding = { ctx.gui_theme().spacing, 0.0f, ctx.gui_theme().spacing, 0.0f };
    box.overflow = Overflow::Clip;
    ctx.begin_box(ctx.id("toolbar"), box);
    ctx.push_id("toolbar");
    h.toolbar_open = true;
    return true;
}

void end_panel_toolbar()
{
    GuiContext& ctx = context();
    DockView& h = host_of(ctx);
    if (!h.toolbar_open)
        throw Error("end_panel_toolbar without begin_panel_toolbar", "every end_panel_toolbar needs a begin_panel_toolbar");
    ctx.pop_id();
    ctx.end_box();
    h.toolbar_open = false;
}

Rect panel_toolbar_rect(std::string_view name)
{
    const PanelBody* body = find_body(host_of(context()), make_panel_id(name));
    return body != nullptr ? body->toolbar : Rect{};
}

bool is_panel_open(std::string_view name)
{
    return is_open(loaded_model().layout, make_panel_id(name));
}

DockResult set_panel_open(std::string_view name, bool open)
{
    DockModel& m = loaded_model();
    const PanelId id = make_panel_id(name);
    if (find_panel(m.panels, id) == nullptr)
        return unknown_panel();
    return open ? open_panel(m.layout, m.panels, id) : close_panel(m.layout, m.panels, id);
}

DockResult toggle_panel(std::string_view name)
{
    return set_panel_open(name, !is_panel_open(name));
}

bool is_panel_focused(std::string_view name)
{
    return loaded_model().focused == make_panel_id(name);
}

void focus_panel(std::string_view name)
{
    DockModel& m = loaded_model();
    const PanelId id = make_panel_id(name);
    if (find_panel(m.panels, id) != nullptr)
        m.focused = id;
}

bool is_group_open(std::string_view group)
{
    DockModel& m = loaded_model();
    return group_open(m.layout, m.panels, make_panel_id(group));
}

DockResult set_group_open(std::string_view group, bool open, GroupEdit edit)
{
    DockModel& m = loaded_model();
    const PanelId id = make_panel_id(group);
    const DockResult result = set_group_open(m.layout, m.panels, id, open);
    if (edit == GroupEdit::SessionOnly)
    {
        static_cast<void>(set_group_open(m.default_layout, m.panels, id, open));
        if (m.autosave != nullptr)
            m.autosave->rebase(m.layout);
        reset(m.history, m.layout);
    }
    return result;
}

void reset_layout()
{
    DockModel& m = loaded_model();
    m.layout = m.default_layout;
    reset(m.history, m.layout);
}

const DockLayout& dock_layout()
{
    return loaded_model().layout;
}

bool set_dock_layout(const DockLayout& layout)
{
    DockModel& m = loaded_model();
    try
    {
        validate(layout, m.panels, ValidateFlags{ has_viewport(m.panels) });
    }
    catch (const Error& error)
    {
        OX_ERROR("set_dock_layout rejected the layout: {}", error.what());
        return false;
    }
    m.layout = layout;
    push(m.history, m.layout);
    return true;
}

bool set_default_layout(const DockLayout& layout)
{
    DockModel& m = context().dock_model();
    try
    {
        validate(layout, m.panels, ValidateFlags{ has_viewport(m.panels) });
    }
    catch (const Error& error)
    {
        OX_ERROR("set_default_layout rejected the layout: {}", error.what());
        return false;
    }
    m.default_layout = layout;
    m.default_set = true;
    return true;
}

DockResult dock_panel(std::string_view panel, std::string_view target, DropZone zone)
{
    DockModel& m = loaded_model();
    const PanelId id = make_panel_id(panel);
    if (find_panel(m.panels, id) == nullptr)
        return unknown_panel();
    if (target.empty())
        return dock_panel(m.layout, m.panels, id, dock_root(), zone);
    int32_t node = k_no_node;
    uint32_t index = 0;
    if (!tab_location(m.layout, make_panel_id(target), node, index))
        return DockResult{ false, DockReason::TargetInvalid };
    return dock_panel(m.layout, m.panels, id, DockTarget{ node }, zone);
}

DockResult float_panel(std::string_view panel, const Rect& rect)
{
    DockModel& m = loaded_model();
    const PanelId id = make_panel_id(panel);
    if (find_panel(m.panels, id) == nullptr)
        return unknown_panel();
    return float_panel(m.layout, m.panels, id, rect);
}

DockResult select_panel(std::string_view panel)
{
    DockModel& m = loaded_model();
    int32_t node = k_no_node;
    uint32_t index = 0;
    if (!tab_location(m.layout, make_panel_id(panel), node, index))
        return find_panel(m.panels, make_panel_id(panel)) == nullptr ? unknown_panel() : DockResult{ false, DockReason::NotFound };
    return select_tab(m.layout, node, index);
}

DockResult set_panel_collapsed(std::string_view panel, bool collapsed)
{
    DockModel& m = loaded_model();
    int32_t node = k_no_node;
    uint32_t index = 0;
    if (!tab_location(m.layout, make_panel_id(panel), node, index))
        return find_panel(m.panels, make_panel_id(panel)) == nullptr ? unknown_panel() : DockResult{ false, DockReason::NotFound };
    return set_collapsed(m.layout, m.panels, node, collapsed);
}

void undo_layout()
{
    context().dock_model().history_request = HistoryRequest::Undo;
}

void redo_layout()
{
    context().dock_model().history_request = HistoryRequest::Redo;
}

bool can_undo_layout()
{
    return can_undo(context().dock_model().history);
}

bool can_redo_layout()
{
    return can_redo(context().dock_model().history);
}

void dock_menu()
{
    if (!settings_of<GuiSettings>().docking)
        return;
    DockModel& m = loaded_model();
    if (begin_menu("Panels"))
    {
        for (uint32_t index = 0; index < m.panels.count; ++index)
        {
            const PanelDesc& desc = m.panels.descs[index];
            if (desc.kind == PanelKind::Viewport)
                continue;
            const bool open = is_open(m.layout, desc.id);
            DisabledScope disabled(open && can_close(m.panels, desc.id) != DockReason::None);
            if (menu_item(desc.title, open).clicked)
                static_cast<void>(open ? close_panel(m.layout, m.panels, desc.id) : open_panel(m.layout, m.panels, desc.id));
        }
        end_menu();
    }
    if (menu_item("Reset layout").clicked)
        reset_layout();
    {
        DisabledScope disabled(!can_undo_layout());
        if (menu_item("Undo layout").clicked)
            undo_layout();
    }
    {
        DisabledScope disabled(!can_redo_layout());
        if (menu_item("Redo layout").clicked)
            redo_layout();
    }
}

}
