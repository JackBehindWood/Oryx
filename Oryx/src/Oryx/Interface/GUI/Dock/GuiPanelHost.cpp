#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/GuiPanelHost.h"
#include "Oryx/Interface/GUI/GuiControls.h"
#include "Oryx/Interface/GUI/GuiMenus.h"

namespace oryx::gui
{

namespace
{

constexpr float k_close_size = 14.0f;

PanelHostState& host_of(GuiContext& ctx) { return ctx.panel_host(); }

bool host_active(const PanelHostState& h, const GuiContext& ctx) { return h.in_host && h.frame == ctx.frame(); }

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

void report_unregistered(PanelHostState& h, PanelId id)
{
    for (uint32_t i = 0; i < h.reported_count; ++i)
        if (h.reported[i] == id.hash)
            return;
    if (h.reported_count < k_max_reported_panels)
        h.reported[h.reported_count++] = id.hash;
    OX_WARN("Dock layout names an unregistered panel (hash {:08x}); it is skipped until registered", id.hash);
}

void note(PanelHostState& h, const DockResult& result) { h.result.layout_changed = h.result.layout_changed || result.applied; }

// The last good layout is the only copy the host keeps; a bad one is swapped for it so the frame never throws.
void keep_valid(PanelHostState& h, DockLayout& layout)
{
    if (h.has_good && equal(layout, h.last_good))
        return;
    try
    {
        validate(layout, h.panels, ValidateFlags{ h.options.require_viewport });
        h.last_good = layout;
        h.has_good = true;
    }
    catch (const Error& error)
    {
        OX_ERROR("Dock layout rejected, keeping the last good one: {}", error.what());
        layout = h.has_good ? h.last_good : DockLayout{};
        h.last_good = layout;
        h.has_good = true;
    }
}

DockMetrics metrics_of(const GuiTheme& theme)
{
    const ImStyle& tab = theme.tab;
    const float scale = tab.text_height / 16.0f;
    DockMetrics metrics;
    metrics.strip_height = tab.text_height + tab.padding.top + tab.padding.bottom;
    metrics.splitter = 4.0f * scale;
    metrics.tab_min_width = 48.0f * scale;
    metrics.tab_max_width = 160.0f * scale;
    metrics.root_edge = 12.0f * scale;
    metrics.strip_button = metrics.strip_height;
    return metrics;
}

LayoutStyle placed(const PanelHostState& h, const Rect& rect)
{
    LayoutStyle style;
    style.width = fixed(rect.size[0]);
    style.height = fixed(rect.size[1]);
    style.floating = { true, AttachPoint::TopLeft, AttachPoint::TopLeft, FloatTarget::Element, h.host_id, { rect.min[0] - h.host_rect.min[0], rect.min[1] - h.host_rect.min[1] } };
    return style;
}

// Opens a filled floating box at `rect`; the caller closes it with end_box.
uint32_t open_fill(GuiContext& ctx, const PanelHostState& h, const Rect& rect, const ImStyle& style, const Colour& fill, bool border)
{
    const uint32_t index = ctx.begin_box(ImId{}, placed(h, rect));
    BoxPaint& paint = ctx.layout().node(index).paint;
    im::paint_surface(paint, style, fill);
    if (!border)
        paint.border_width = 0.0f;
    return index;
}

const PanelBody* find_body(const PanelHostState& h, PanelId id)
{
    for (uint32_t i = 0; i < h.body_count; ++i)
        if (h.bodies[i].panel == id)
            return &h.bodies[i];
    return nullptr;
}

void add_body(PanelHostState& h, PanelId id, const Rect& rect, bool viewport)
{
    if (h.body_count < k_max_panels)
        h.bodies[h.body_count++] = PanelBody{ id, rect, viewport };
}

void draw_body(GuiContext& ctx, PanelHostState& h, const DockNode& node, const SolvedNode& solved)
{
    if (node.collapsed != 0 || node.selected >= node.count || is_empty(solved.body))
        return;
    const PanelId id = node.tabs[node.selected];
    const PanelDesc* desc = find_panel(h.panels, id);
    if (desc == nullptr)
    {
        report_unregistered(h, id);
        return;
    }
    add_body(h, id, solved.body, desc->kind == PanelKind::Viewport);
    if (desc->kind == PanelKind::Viewport)
        return;
    const GuiTheme& theme = ctx.gui_theme();
    const bool focused = h.focused == id;
    open_fill(ctx, h, solved.body, theme.panel, theme.panel.background, false);
    if (focused)
    {
        BoxPaint& paint = ctx.layout().current().paint;
        paint.border_width = 1.0f;
        paint.border = theme.panel.accent;
    }
    ctx.end_box();
    IdScope scope(ctx, ctx.index_id(id.hash));
    if (ctx.item(ctx.id("body"), solved.body).pressed)
        h.focused = id;
}

void draw_tab(GuiContext& ctx, PanelHostState& h, int32_t node_index, const DockNode& node, uint32_t index, const Rect& rect, PanelId& pending_close)
{
    const PanelId id = node.tabs[index];
    const PanelDesc* desc = find_panel(h.panels, id);
    if (desc == nullptr)
    {
        report_unregistered(h, id);
        return;
    }
    const ImStyle& style = ctx.gui_theme().tab;
    const bool chosen = index == node.selected;
    const bool closable = can_close(h.panels, id) == DockReason::None;
    const float pad = style.padding.right * 0.5f;
    const Rect close_rect{ Vec2f(rect.min[0] + rect.size[0] - pad - k_close_size, rect.min[1] + (rect.size[1] - k_close_size) * 0.5f), Vec2f(k_close_size, k_close_size) };

    IdScope scope(ctx, ctx.index_id(id.hash));
    const ItemState tab = ctx.item(ctx.id("tab"), rect);
    ItemState close;
    if (closable)
        close = ctx.item(ctx.id("close"), close_rect);
    if (tab.hovered)
        ctx.request_cursor(CursorShape::Hand);

    LayoutStyle box = placed(h, rect);
    box.padding = style.padding;
    box.padding.right += closable ? k_close_size + pad : 0.0f;
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
    }
    if (closable)
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
    if (chosen)
    {
        LayoutStyle underline;
        underline.width = grow();
        underline.height = fixed(2.0f);
        underline.floating = { true, AttachPoint::BottomLeft, AttachPoint::BottomLeft, FloatTarget::Parent, {}, { 0.0f, 0.0f } };
        const uint32_t line_index = ctx.begin_box(ImId{}, underline);
        BoxPaint& paint = ctx.layout().node(line_index).paint;
        paint.has_fill = true;
        paint.fill = style.accent;
        ctx.end_box();
    }
    ctx.end_box();
    tooltip(tab, desc->title);

    if (tab.pressed && !close.hovered)
    {
        h.focused = id;
        if (!chosen)
            note(h, select_tab(*h.layout, node_index, index));
    }
    if (close.clicked)
        pending_close = id;
}

void draw_strip(GuiContext& ctx, PanelHostState& h, int32_t node_index, PanelId& pending_close)
{
    const DockNode& node = h.layout->nodes[node_index];
    const SolvedNode& solved = h.solved.nodes[node_index];
    const GuiTheme& theme = ctx.gui_theme();
    const ImStyle& style = theme.tab;
    const bool collapsed = node.collapsed != 0;

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
            note(h, set_collapsed(*h.layout, h.panels, node_index, !collapsed));
    }
}

void draw_splitter(GuiContext& ctx, PanelHostState& h, int32_t node_index)
{
    const DockNode& node = h.layout->nodes[node_index];
    const SolvedNode& solved = h.solved.nodes[node_index];
    const ImStyle& style = ctx.gui_theme().panel;
    ItemState state;
    if (can_resize_split(*h.layout, h.panels, node_index))
    {
        IdScope scope(ctx, ctx.index_id(static_cast<uint64_t>(node_index)));
        const ImId id = ctx.id("splitter");
        state = ctx.item(id, solved.splitter);
        const ItemDrag drag = ctx.item_drag(id);
        const bool horizontal = node.axis == DockAxis::Horizontal;
        const uint32_t axis = horizontal ? 0 : 1;
        if (state.hovered || state.held)
            ctx.request_cursor(horizontal ? CursorShape::ResizeHorizontal : CursorShape::ResizeVertical);
        h.result.interacting = h.result.interacting || state.held;
        if ((drag.started || drag.dragging) && drag.delta[axis] != 0.0f)
        {
            const float first = h.solved.nodes[node.first].rect.size[axis] + drag.delta[axis];
            const float total = solved.rect.size[axis] - solved.splitter.size[axis];
            if (total > 0.0f)
            {
                if (node.mode == DockSizeMode::Ratio)
                    note(h, set_split(*h.layout, h.panels, node_index, node.mode, first / total, node.points));
                else if (node.mode == DockSizeMode::FixedFirst)
                    note(h, set_split(*h.layout, h.panels, node_index, node.mode, node.ratio, first));
                else
                    note(h, set_split(*h.layout, h.panels, node_index, node.mode, node.ratio, total - first));
            }
        }
    }
    open_fill(ctx, h, solved.splitter, style, state.held ? style.accent : im::interaction_fill(style, state, style.border), false);
    ctx.end_box();
}

void draw_empty(GuiContext& ctx, PanelHostState& h)
{
    LayoutStyle box;
    box.direction = Direction::Column;
    box.align_x = Align::Centre;
    box.gap = spacing();
    box.floating = { true, AttachPoint::Centre, AttachPoint::Centre, FloatTarget::Element, h.host_id, {} };
    ctx.begin_box("panel_host_empty", box);
    ctx.push_id("panel_host_empty");
    label("No panels open");
    if (h.options.default_layout != nullptr && button("Reset layout").clicked)
    {
        *h.layout = *h.options.default_layout;
        h.result.layout_changed = true;
    }
    ctx.pop_id();
    ctx.end_box();
}

void draw_host(GuiContext& ctx, PanelHostState& h)
{
    h.solved = solve(*h.layout, h.panels, metrics_of(ctx.gui_theme()), h.host_rect, static_cast<uint8_t>(ctx.input().surface));
    const int32_t root = h.layout->roots[ctx.input().surface];
    if (root == k_no_node)
    {
        draw_empty(ctx, h);
        return;
    }
    ctx.push_id("panel_host");
    PanelId pending_close;
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
    ctx.pop_id();
    if (is_valid(pending_close))
        note(h, close_panel(*h.layout, h.panels, pending_close));
}

} // namespace

bool register_panel(std::string_view name, const PanelOptions& options)
{
    PanelHostState& h = host_of(context());
    if (!add_panel(h.panels, name, options.title.empty() ? name : options.title, options.kind, options.min_w, options.min_h))
        return false;
    find_panel(h.panels, make_panel_id(name))->flags = PanelFlags{ flags_of(options) };
    return true;
}

const PanelTable& panels()
{
    return host_of(context()).panels;
}

bool dock_only_in(std::string_view panel, std::string_view target)
{
    return add_dock_only(host_of(context()).panels, make_panel_id(panel), make_panel_id(target));
}

bool never_dock_in(std::string_view panel, std::string_view target)
{
    return add_dock_never(host_of(context()).panels, make_panel_id(panel), make_panel_id(target));
}

void begin_panel_host(DockLayout& layout, const PanelHostOptions& options)
{
    GuiContext& ctx = context();
    PanelHostState& h = host_of(ctx);
    if (host_active(h, ctx))
        throw Error("begin_panel_host called inside a panel host", "call end_panel_host first");
    h.in_host = true;
    h.frame = ctx.frame();
    h.dock_body_open = false;
    h.depth = 0;
    h.layout = &layout;
    h.options = options;
    h.result = {};
    h.body_count = 0;
    h.solved.node_count = 0;
    keep_valid(h, layout);

    h.host_id = ctx.id("panel_host");
    LayoutStyle area;
    area.width = grow();
    area.height = grow();
    ctx.begin_box(h.host_id, area);
    ctx.end_box();
    const bool known = ctx.layout_rect(h.host_id, h.host_rect);
    if (known && ctx.input().surface < k_max_dock_surfaces)
        draw_host(ctx, h);
}

void end_panel_host()
{
    GuiContext& ctx = context();
    PanelHostState& h = host_of(ctx);
    if (!host_active(h, ctx))
        throw Error("end_panel_host without begin_panel_host", "every end_panel_host needs a begin_panel_host");
    if (h.depth != 0)
        throw Error("end_panel_host with a panel still open", "every begin_panel needs an end_panel");
    if (h.result.layout_changed)
    {
        h.last_good = *h.layout;
        h.has_good = true;
    }
    h.in_host = false;
    h.layout = nullptr;
}

bool begin_panel(std::string_view name, const PanelOptions& options)
{
    GuiContext& ctx = context();
    PanelHostState& h = host_of(ctx);
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
    if (find_panel(h.panels, id) == nullptr)
        std::ignore = register_panel(name, options);
    const PanelBody* body = find_body(h, id);
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
    const LayoutStyle frame = placed(h, body->rect);
    box.width = frame.width;
    box.height = frame.height;
    box.floating = frame.floating;
    ctx.begin_box(name, box);
    ctx.push_id(name);
    begin_scroll("content");
    h.dock_body_open = true;
    h.stack[h.depth++] = PanelCall::Dock;
    return true;
}

void end_panel()
{
    GuiContext& ctx = context();
    PanelHostState& h = host_of(ctx);
    if (h.depth == 0)
        throw Error("end_panel without begin_panel", "every end_panel needs a begin_panel");
    const PanelCall call = h.stack[--h.depth];
    if (call == PanelCall::Plain)
    {
        im::end_panel(ctx);
    }
    else if (call == PanelCall::Dock)
    {
        end_scroll();
        ctx.pop_id();
        ctx.end_box();
        h.dock_body_open = false;
    }
}

Rect viewport_rect(std::string_view name)
{
    const PanelBody* body = find_body(host_of(context()), make_panel_id(name));
    return body != nullptr && body->viewport ? body->rect : Rect{};
}

PanelHostResult panel_host_result()
{
    return host_of(context()).result;
}

PanelId focused_panel()
{
    return host_of(context()).focused;
}

void set_focused_panel(PanelId panel)
{
    host_of(context()).focused = panel;
}

}
