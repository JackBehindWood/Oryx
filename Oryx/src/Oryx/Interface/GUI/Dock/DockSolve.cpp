#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/DockSolve.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"

namespace oryx::gui
{

namespace
{

constexpr uint32_t k_max_depth = k_max_dock_nodes + 1;

struct MinSize
{
    float w = 0.0f;
    float h = 0.0f;
};

struct Solver
{
    const DockLayout& layout;
    const PanelTable& panels;
    const DockMetrics& metrics;
    SolvedLayout& out;
    MinSize mins[k_max_dock_nodes];

    bool valid(int32_t node) const { return node >= 0 && static_cast<uint32_t>(node) < layout.node_count; }
    const PanelDesc* selected_desc(const DockNode& n) const { return n.selected < n.count ? find_panel(panels, n.tabs[n.selected]) : nullptr; }

    float toolbar_height(const DockNode& n) const
    {
        const PanelDesc* desc = selected_desc(n);
        return metrics.style.toolbars && desc != nullptr && desc->toolbar && n.collapsed == 0 ? metrics.toolbar_height : 0.0f;
    }

    bool collapsed_tabs(int32_t node) const { return valid(node) && layout.nodes[node].kind == DockNodeKind::Tabs && layout.nodes[node].collapsed != 0; }

    MinSize compute_min(int32_t node, uint32_t depth)
    {
        if (!valid(node) || depth > k_max_depth)
            return {};
        const DockNode& n = layout.nodes[node];
        MinSize result;
        if (n.kind == DockNodeKind::Tabs)
        {
            if (n.collapsed != 0)
            {
                result = { metrics.strip_height, metrics.strip_height };
            }
            else
            {
                float w = 0.0f;
                float h = 0.0f;
                for (uint32_t t = 0; t < n.count && t < k_max_dock_tabs; ++t)
                {
                    const PanelDesc* desc = find_panel(panels, n.tabs[t]);
                    if (desc == nullptr)
                        continue;
                    w = math::max(w, desc->min_w);
                    h = math::max(h, desc->min_h);
                }
                result = { w, metrics.strip_height + toolbar_height(n) + h };
            }
        }
        else if (n.kind == DockNodeKind::Split)
        {
            const MinSize a = compute_min(n.first, depth + 1);
            const MinSize b = compute_min(n.second, depth + 1);
            if (n.axis == DockAxis::Horizontal)
                result = { a.w + metrics.splitter + b.w, math::max(a.h, b.h) };
            else
                result = { math::max(a.w, b.w), a.h + metrics.splitter + b.h };
        }
        mins[node] = result;
        return result;
    }

    void place_tabs(int32_t node, const Rect& rect)
    {
        const DockNode& n = layout.nodes[node];
        SolvedNode& s = out.nodes[node];
        const float strip_h = math::min(metrics.strip_height, rect.size[1]);
        const float tool_h = math::min(toolbar_height(n), rect.size[1] - strip_h);
        const float stack_h = strip_h + tool_h;
        const bool bottom = metrics.style.tab_position == TabPosition::Bottom;
        const PanelDesc* desc = selected_desc(n);
        const bool above = tool_h > 0.0f && desc != nullptr && resolve_toolbar_placement(metrics.style, *desc, n.count) == ToolbarPlacement::AboveTabs;
        const float stack_y = bottom ? rect.min[1] + rect.size[1] - stack_h : rect.min[1];
        const float strip_y = stack_y + (above ? tool_h : 0.0f);
        s.strip = Rect{ Vec2f(rect.min[0], strip_y), Vec2f(rect.size[0], strip_h) };
        if (tool_h > 0.0f)
            s.toolbar = Rect{ Vec2f(rect.min[0], stack_y + (above ? 0.0f : strip_h)), Vec2f(rect.size[0], tool_h) };
        s.body = Rect{ Vec2f(rect.min[0], bottom ? rect.min[1] : rect.min[1] + stack_h), Vec2f(rect.size[0], n.collapsed != 0 ? 0.0f : rect.size[1] - stack_h) };

        const uint32_t count = math::min<uint32_t>(n.count, k_max_dock_tabs);
        float avail = rect.size[0];
        bool collapsible = count > 0 && metrics.strip_button > 0.0f && avail > metrics.strip_button;
        for (uint32_t t = 0; t < count && collapsible; ++t)
            collapsible = can_collapse(panels, n.tabs[t]) == DockReason::None;
        if (collapsible)
        {
            avail -= metrics.strip_button;
            s.collapse_button = Rect{ Vec2f(rect.min[0] + avail, strip_y), Vec2f(metrics.strip_button, strip_h) };
        }
        if (count == 0 || !(avail > 0.0f))
            return;

        float tab_w = avail / static_cast<float>(count);
        uint32_t visible = count;
        uint32_t first = 0;
        if (tab_w >= metrics.tab_min_width)
        {
            tab_w = math::min(tab_w, metrics.tab_max_width);
        }
        else
        {
            tab_w = math::min(metrics.tab_min_width, avail);
            visible = math::clamp<uint32_t>(static_cast<uint32_t>(avail / tab_w), 1u, count);
            if (n.selected >= visible)
                first = n.selected - visible + 1u;
        }
        s.first_visible = static_cast<uint8_t>(first);
        s.visible_count = static_cast<uint8_t>(visible);
        for (uint32_t i = 0; i < visible; ++i)
            s.tab_rects[first + i] = Rect{ Vec2f(rect.min[0] + tab_w * static_cast<float>(i), strip_y), Vec2f(tab_w, strip_h) };
    }

    void place(int32_t node, Rect rect, uint32_t depth)
    {
        if (!valid(node) || depth > k_max_depth)
            return;
        rect.size = Vec2f(math::max(rect.size[0], 0.0f), math::max(rect.size[1], 0.0f));
        out.nodes[node].rect = rect;
        const DockNode& n = layout.nodes[node];
        if (n.kind == DockNodeKind::Tabs)
        {
            place_tabs(node, rect);
            return;
        }
        if (n.kind != DockNodeKind::Split)
            return;

        const bool horizontal = n.axis == DockAxis::Horizontal;
        const float extent = horizontal ? rect.size[0] : rect.size[1];
        const float gap = math::min(metrics.splitter, extent);
        const float total = extent - gap;
        const float min_a = horizontal ? mins[n.first].w : mins[n.first].h;
        const float min_b = horizontal ? mins[n.second].w : mins[n.second].h;
        const bool collapsed_a = collapsed_tabs(n.first);
        const bool collapsed_b = collapsed_tabs(n.second);

        float preferred = 0.0f;
        if (collapsed_a != collapsed_b)
            preferred = collapsed_a ? metrics.strip_height : total - metrics.strip_height;
        else if (n.mode == DockSizeMode::Ratio)
            preferred = total * n.ratio;
        else if (n.mode == DockSizeMode::FixedFirst)
            preferred = n.points;
        else
            preferred = total - n.points;

        float a = 0.0f;
        if (min_a + min_b > total)
            a = total * min_a / (min_a + min_b);
        else
            a = math::clamp(preferred, min_a, total - min_b);
        const float b = total - a;

        out.nodes[node].splitter = horizontal ? Rect{ Vec2f(rect.min[0] + a, rect.min[1]), Vec2f(gap, rect.size[1]) } : Rect{ Vec2f(rect.min[0], rect.min[1] + a), Vec2f(rect.size[0], gap) };
        if (horizontal)
        {
            place(n.first, Rect{ rect.min, Vec2f(a, rect.size[1]) }, depth + 1);
            place(n.second, Rect{ Vec2f(rect.min[0] + a + gap, rect.min[1]), Vec2f(b, rect.size[1]) }, depth + 1);
        }
        else
        {
            place(n.first, Rect{ rect.min, Vec2f(rect.size[0], a) }, depth + 1);
            place(n.second, Rect{ Vec2f(rect.min[0], rect.min[1] + a + gap), Vec2f(rect.size[0], b) }, depth + 1);
        }
    }
};

// The rect a docked panel would take, computed exactly as the split's Ratio sizing does.
Rect edge_preview(const Rect& rect, DropZone zone, const DockMetrics& metrics)
{
    const float gap_w = math::min(metrics.splitter, rect.size[0]);
    const float gap_h = math::min(metrics.splitter, rect.size[1]);
    const float w = (rect.size[0] - gap_w) * k_dock_new_ratio;
    const float h = (rect.size[1] - gap_h) * k_dock_new_ratio;
    switch (zone)
    {
    case DropZone::Left: return Rect{ rect.min, Vec2f(w, rect.size[1]) };
    case DropZone::Right: return Rect{ Vec2f(rect.min[0] + rect.size[0] - w, rect.min[1]), Vec2f(w, rect.size[1]) };
    case DropZone::Top: return Rect{ rect.min, Vec2f(rect.size[0], h) };
    case DropZone::Bottom: return Rect{ Vec2f(rect.min[0], rect.min[1] + rect.size[1] - h), Vec2f(rect.size[0], h) };
    case DropZone::Centre: break;
    }
    return rect;
}

// Distances are left, right, top, bottom; the first nearest wins a tie, so the result is deterministic.
DropZone nearest_edge(float left, float right, float top, float bottom, float& distance)
{
    DropZone zone = DropZone::Left;
    distance = left;
    if (right < distance)
    {
        zone = DropZone::Right;
        distance = right;
    }
    if (top < distance)
    {
        zone = DropZone::Top;
        distance = top;
    }
    if (bottom < distance)
    {
        zone = DropZone::Bottom;
        distance = bottom;
    }
    return zone;
}

}

ToolbarPlacement resolve_toolbar_placement(const DockStyle& style, const PanelDesc& panel, uint32_t tab_count)
{
    if (style.toolbar_placement != ToolbarPlacement::Auto)
        return style.toolbar_placement;
    if (panel.toolbar_placement != ToolbarPlacement::Auto)
        return panel.toolbar_placement;
    return tab_count <= 1 ? ToolbarPlacement::AboveTabs : ToolbarPlacement::BelowTabs;
}

SolvedLayout solve(const DockLayout& layout, const PanelTable& panels, const DockMetrics& metrics, const Rect& surface_rect, uint8_t surface)
{
    SolvedLayout out;
    out.surface = surface;
    out.surface_rect = Rect{ surface_rect.min, Vec2f(math::max(surface_rect.size[0], 0.0f), math::max(surface_rect.size[1], 0.0f)) };
    out.metrics = metrics;
    out.node_count = math::min(layout.node_count, k_max_dock_nodes);
    out.float_count = math::min(layout.float_count, k_max_dock_floats);

    Solver solver{ layout, panels, metrics, out, {} };
    const int32_t root = surface < k_max_dock_surfaces ? layout.roots[surface] : k_no_node;
    if (root != k_no_node)
    {
        solver.compute_min(root, 0);
        solver.place(root, out.surface_rect, 0);
    }

    for (uint32_t f = 0; f < out.float_count; ++f)
    {
        const DockFloat& fl = layout.floats[f];
        if (fl.surface != surface)
            continue;
        const PanelDesc* desc = find_panel(panels, fl.panel);
        const float min_w = desc != nullptr ? desc->min_w : 0.0f;
        const float tool_h = metrics.style.toolbars && desc != nullptr && desc->toolbar ? metrics.toolbar_height : 0.0f;
        const float min_h = (desc != nullptr ? desc->min_h : 0.0f) + metrics.strip_height + tool_h;
        const float w = math::min(math::max(fl.rect.size[0], min_w), out.surface_rect.size[0]);
        const float h = math::min(math::max(fl.rect.size[1], min_h), out.surface_rect.size[1]);
        const float x = math::clamp(fl.rect.min[0], out.surface_rect.min[0], out.surface_rect.min[0] + out.surface_rect.size[0] - w);
        const float y = math::clamp(fl.rect.min[1], out.surface_rect.min[1], out.surface_rect.min[1] + out.surface_rect.size[1] - h);
        out.floats[f] = Rect{ Vec2f(x, y), Vec2f(w, h) };
        const float title_h = math::min(metrics.strip_height, h);
        const float bar_h = math::min(tool_h, h - title_h);
        SolvedFloat& parts = out.float_parts[f];
        parts.title = Rect{ Vec2f(x, y), Vec2f(w, title_h) };
        if (bar_h > 0.0f)
            parts.toolbar = Rect{ Vec2f(x, y + title_h), Vec2f(w, bar_h) };
        parts.body = Rect{ Vec2f(x, y + title_h + bar_h), Vec2f(w, h - title_h - bar_h) };
    }
    return out;
}

DropTarget drop_target(const DockLayout& layout, const SolvedLayout& solved, const Vec2f& pointer)
{
    DropTarget result;
    const Rect& surface = solved.surface_rect;
    if (!contains(surface, pointer))
        return result;

    const int32_t root = solved.surface < k_max_dock_surfaces ? layout.roots[solved.surface] : k_no_node;
    if (root == k_no_node)
    {
        result = { true, k_dock_root, DropZone::Centre, surface };
        return result;
    }

    const DockMetrics& metrics = solved.metrics;
    int32_t hit = k_no_node;
    for (uint32_t n = 0; n < solved.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && contains(solved.nodes[n].rect, pointer))
        {
            hit = static_cast<int32_t>(n);
            break;
        }

    // A strip wins over the window-edge band so the top row of tabs stays a tab drop.
    if (hit != k_no_node && contains(solved.nodes[hit].strip, pointer))
        return { true, hit, DropZone::Centre, solved.nodes[hit].rect };

    float edge_distance = 0.0f;
    const DropZone edge = nearest_edge(pointer[0] - surface.min[0], surface.min[0] + surface.size[0] - pointer[0], pointer[1] - surface.min[1], surface.min[1] + surface.size[1] - pointer[1], edge_distance);
    if (edge_distance < metrics.root_edge)
        return { true, k_dock_root, edge, edge_preview(surface, edge, metrics) };

    if (hit == k_no_node)
        return result;
    const SolvedNode& s = solved.nodes[hit];
    result = { true, hit, DropZone::Centre, s.rect };
    if (!(s.body.size[0] > 0.0f) || !(s.body.size[1] > 0.0f))
        return result;

    const float u = (pointer[0] - s.body.min[0]) / s.body.size[0];
    const float v = (pointer[1] - s.body.min[1]) / s.body.size[1];
    float band_distance = 0.0f;
    const DropZone zone = nearest_edge(u, 1.0f - u, v, 1.0f - v, band_distance);
    if (band_distance < metrics.edge_band)
    {
        result.zone = zone;
        result.preview = edge_preview(s.rect, zone, metrics);
    }
    return result;
}

namespace
{

int32_t tab_node_of(const DockLayout& layout, PanelId panel)
{
    for (uint32_t n = 0; n < layout.node_count && n < k_max_dock_nodes; ++n)
    {
        const DockNode& node = layout.nodes[n];
        if (node.kind != DockNodeKind::Tabs)
            continue;
        for (uint32_t t = 0; t < node.count && t < k_max_dock_tabs; ++t)
            if (node.tabs[t] == panel)
                return static_cast<int32_t>(n);
    }
    return k_no_node;
}

bool is_floating(const DockLayout& layout, PanelId panel)
{
    for (uint32_t f = 0; f < layout.float_count && f < k_max_dock_floats; ++f)
        if (layout.floats[f].panel == panel)
            return true;
    return false;
}

// The gap before the first visible tab whose midpoint lies right of x, counted in tab indices.
uint32_t insertion_slot(const DockNode& node, const SolvedNode& solved, float x)
{
    uint32_t slot = solved.first_visible;
    const uint32_t end = math::min<uint32_t>(solved.first_visible + solved.visible_count, node.count);
    for (uint32_t i = solved.first_visible; i < end; ++i)
        if (x >= solved.tab_rects[i].min[0] + solved.tab_rects[i].size[0] * 0.5f)
            slot = i + 1;
    return slot;
}

Rect insertion_marker(const DockNode& node, const SolvedNode& solved, uint32_t slot)
{
    if (solved.visible_count == 0)
        return {};
    const uint32_t first = solved.first_visible;
    const uint32_t end = math::min<uint32_t>(first + solved.visible_count, node.count);
    float x = solved.tab_rects[first].min[0];
    if (slot > first)
    {
        const Rect& tab = solved.tab_rects[math::min(slot, end) - 1u];
        x = tab.min[0] + tab.size[0];
    }
    return Rect{ Vec2f(x - 1.0f, solved.strip.min[1]), Vec2f(2.0f, solved.strip.size[1]) };
}

Rect float_preview(const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer)
{
    const PanelDesc* desc = find_panel(panels, panel);
    const DockMetrics& metrics = solved.metrics;
    const float tool_h = metrics.style.toolbars && desc != nullptr && desc->toolbar ? metrics.toolbar_height : 0.0f;
    const Rect& surface = solved.surface_rect;
    const float w = math::min(math::max(k_dock_float_width, desc != nullptr ? desc->min_w : 0.0f), surface.size[0]);
    const float h = math::min(math::max(k_dock_float_height, (desc != nullptr ? desc->min_h : 0.0f) + metrics.strip_height + tool_h), surface.size[1]);
    const float x = math::clamp(pointer[0] - w * 0.5f, surface.min[0], surface.min[0] + surface.size[0] - w);
    const float y = math::clamp(pointer[1] - metrics.strip_height * 0.5f, surface.min[1], surface.min[1] + surface.size[1] - h);
    return Rect{ Vec2f(x, y), Vec2f(w, h) };
}

DropPlan refused(DockReason reason, int32_t node = k_no_node, DropZone zone = DropZone::Centre)
{
    DropPlan plan;
    plan.action = DropAction::Cancel;
    plan.reason = reason;
    plan.node = node;
    plan.zone = zone;
    return plan;
}

}

DropPlan resolve_drop(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer, bool float_only)
{
    const int32_t source = tab_node_of(layout, panel);
    const bool floating = is_floating(layout, panel);
    if (source == k_no_node && !floating)
        return refused(DockReason::NotFound);

    const bool inside = contains(solved.surface_rect, pointer);
    if (!float_only && inside)
    {
        if (source != k_no_node && contains(solved.nodes[source].strip, pointer))
        {
            const DockReason reason = can_reorder(panels, panel);
            if (reason != DockReason::None)
                return refused(reason, source);
            const DockNode& node = layout.nodes[source];
            const uint32_t insertion = insertion_slot(node, solved.nodes[source], pointer[0]);
            uint32_t slot = insertion;
            uint32_t current = 0;
            while (current < node.count && node.tabs[current] != panel)
                ++current;
            slot = slot > current ? slot - 1u : slot;
            DropPlan plan;
            plan.action = slot == current ? DropAction::None : DropAction::Reorder;
            plan.node = source;
            plan.slot = slot;
            if (plan.action == DropAction::Reorder)
                plan.marker = insertion_marker(node, solved.nodes[source], insertion);
            return plan;
        }

        const DropTarget target = drop_target(layout, solved, pointer);
        if (target.valid)
        {
            if (target.node == source && source != k_no_node && (target.zone == DropZone::Centre || layout.nodes[source].count == 1))
                return DropPlan{};
            // A float only docks into a strip or an edge zone, so it can be moved across a body without sticking.
            if (floating && target.zone == DropZone::Centre && target.node != k_dock_root && !contains(solved.nodes[target.node].strip, pointer))
                return DropPlan{};
            const DockReason reason = can_dock_into(layout, panels, panel, target.node);
            if (reason != DockReason::None)
                return refused(reason, target.node, target.zone);
            DropPlan plan;
            plan.action = DropAction::Dock;
            plan.node = target.node;
            plan.zone = target.zone;
            plan.preview = target.preview;
            plan.slot = target.node != k_dock_root ? layout.nodes[target.node].count : 0u;
            if (target.zone == DropZone::Centre && target.node != k_dock_root && contains(solved.nodes[target.node].strip, pointer))
            {
                plan.slot = insertion_slot(layout.nodes[target.node], solved.nodes[target.node], pointer[0]);
                plan.marker = insertion_marker(layout.nodes[target.node], solved.nodes[target.node], plan.slot);
            }
            return plan;
        }
        return refused(DockReason::TargetInvalid);
    }

    if (floating)
        return DropPlan{};
    const DockReason reason = can_float(panels, panel);
    if (reason != DockReason::None)
        return refused(reason);
    DropPlan plan;
    plan.action = DropAction::Float;
    plan.preview = float_preview(panels, solved, panel, pointer);
    return plan;
}

}
