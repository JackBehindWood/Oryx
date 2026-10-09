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
    // An all-collapsed root has nothing to give the space to, so it is laid out expanded.
    bool ignore_collapse = false;

    bool valid(int32_t node) const { return node >= 0 && static_cast<uint32_t>(node) < layout.node_count; }
    const PanelDesc* selected_desc(const DockNode& n) const { return n.selected < n.count ? find_panel(panels, n.tabs[n.selected]) : nullptr; }

    float toolbar_height(const DockNode& n) const
    {
        const PanelDesc* desc = selected_desc(n);
        return metrics.style.toolbars && desc != nullptr && desc->toolbar && !is_collapsed(n) ? metrics.toolbar_height : 0.0f;
    }

    bool is_collapsed(const DockNode& n) const { return !ignore_collapse && n.collapsed != 0; }
    bool collapsed_side(int32_t node) const { return !ignore_collapse && all_collapsed(layout, node); }

    MinSize compute_min(int32_t node, uint32_t depth)
    {
        if (!valid(node) || depth > k_max_depth)
            return {};
        const DockNode& n = layout.nodes[node];
        MinSize result;
        if (n.kind == DockNodeKind::Tabs)
        {
            if (is_collapsed(n))
            {
                result = { rail_width(metrics), metrics.strip_height };
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

    void place_tabs(int32_t node, const Rect& rect, bool rail)
    {
        const DockNode& n = layout.nodes[node];
        SolvedNode& s = out.nodes[node];
        const bool collapsed = is_collapsed(n);
        s.collapsed = collapsed;
        s.rail = collapsed && rail;
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
        s.body = Rect{ Vec2f(rect.min[0], bottom ? rect.min[1] : rect.min[1] + stack_h), Vec2f(rect.size[0], collapsed ? 0.0f : rect.size[1] - stack_h) };

        const uint32_t count = math::min<uint32_t>(n.count, k_max_dock_tabs);
        float avail = rect.size[0];
        bool collapsible = count > 0 && metrics.strip_button > 0.0f && avail > metrics.strip_button;
        for (uint32_t t = 0; t < count && collapsible; ++t)
            collapsible = can_collapse(panels, n.tabs[t]) == DockReason::None;
        // A collapsed node always keeps its expander, whatever its width or permissions, so it can never be stranded.
        if (collapsed)
        {
            const float button = math::min(expander_size(metrics), avail);
            avail -= button;
            s.collapse_button = Rect{ Vec2f(rect.min[0] + avail, strip_y), Vec2f(button, strip_h) };
        }
        else if (collapsible)
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

    void place(int32_t node, Rect rect, uint32_t depth, bool rail)
    {
        if (!valid(node) || depth > k_max_depth)
            return;
        rect.size = Vec2f(math::max(rect.size[0], 0.0f), math::max(rect.size[1], 0.0f));
        out.nodes[node].rect = rect;
        const DockNode& n = layout.nodes[node];
        if (n.kind == DockNodeKind::Tabs)
        {
            place_tabs(node, rect, rail);
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
        const bool collapsed_a = collapsed_side(n.first);
        const bool collapsed_b = collapsed_side(n.second);

        float preferred = 0.0f;
        // Only a vertical split shrinks a collapsed side to its strip; a horizontal one keeps the user's width (never below the rail minimum).
        if (!horizontal && collapsed_a != collapsed_b)
            preferred = collapsed_a ? min_a : total - min_b;
        else if (!horizontal && collapsed_a)
            preferred = min_a;
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
            place(n.first, Rect{ rect.min, Vec2f(a, rect.size[1]) }, depth + 1, true);
            place(n.second, Rect{ Vec2f(rect.min[0] + a + gap, rect.min[1]), Vec2f(b, rect.size[1]) }, depth + 1, true);
        }
        else
        {
            place(n.first, Rect{ rect.min, Vec2f(rect.size[0], a) }, depth + 1, false);
            place(n.second, Rect{ Vec2f(rect.min[0], rect.min[1] + a + gap), Vec2f(rect.size[0], b) }, depth + 1, false);
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

// The strip a drop beside the whole tree takes: the panel's fixed dock_size in points (never over half the surface), else the same ratio rect as an inner split.
Rect root_preview(const Rect& surface, DropZone zone, float points, const DockMetrics& metrics)
{
    if (!(points > 0.0f) || zone == DropZone::Centre)
        return edge_preview(surface, zone, metrics);
    const bool vertical_edge = zone == DropZone::Left || zone == DropZone::Right;
    const uint32_t axis = vertical_edge ? 0 : 1;
    const float size = math::min(points, (surface.size[axis] - math::min(metrics.splitter, surface.size[axis])) * 0.5f);
    switch (zone)
    {
    case DropZone::Left: return Rect{ surface.min, Vec2f(size, surface.size[1]) };
    case DropZone::Right: return Rect{ Vec2f(surface.min[0] + surface.size[0] - size, surface.min[1]), Vec2f(size, surface.size[1]) };
    case DropZone::Top: return Rect{ surface.min, Vec2f(surface.size[0], size) };
    case DropZone::Bottom: return Rect{ Vec2f(surface.min[0], surface.min[1] + surface.size[1] - size), Vec2f(surface.size[0], size) };
    case DropZone::Centre: break;
    }
    return surface;
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

Vec2f float_min_size(const PanelTable& panels, PanelId panel, const DockMetrics& metrics)
{
    const PanelDesc* desc = find_panel(panels, panel);
    const float tool_h = metrics.style.toolbars && desc != nullptr && desc->toolbar ? metrics.toolbar_height : 0.0f;
    return Vec2f(desc != nullptr ? desc->min_w : 0.0f, (desc != nullptr ? desc->min_h : 0.0f) + metrics.strip_height + tool_h);
}

Rect clamp_float(const Rect& rect, const Vec2f& min_size, const Rect& surface)
{
    const float w = math::min(math::max(rect.size[0], min_size[0]), surface.size[0]);
    const float h = math::min(math::max(rect.size[1], min_size[1]), surface.size[1]);
    const float x = math::clamp(rect.min[0], surface.min[0], surface.min[0] + surface.size[0] - w);
    const float y = math::clamp(rect.min[1], surface.min[1], surface.min[1] + surface.size[1] - h);
    return Rect{ Vec2f(x, y), Vec2f(w, h) };
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
        solver.ignore_collapse = all_collapsed(layout, root);
        solver.compute_min(root, 0);
        solver.place(root, out.surface_rect, 0, false);
    }

    for (uint32_t f = 0; f < out.float_count; ++f)
    {
        const DockFloat& fl = layout.floats[f];
        if (fl.surface != surface)
            continue;
        const PanelDesc* desc = find_panel(panels, fl.panel);
        const float tool_h = metrics.style.toolbars && desc != nullptr && desc->toolbar ? metrics.toolbar_height : 0.0f;
        const Rect fitted = clamp_float(fl.rect, float_min_size(panels, fl.panel, metrics), out.surface_rect);
        const float x = fitted.min[0];
        const float y = fitted.min[1];
        const float w = fitted.size[0];
        const float h = fitted.size[1];
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

namespace
{

bool same_subtree(const DockLayout& a, int32_t ia, const DockLayout& b, int32_t ib, uint32_t depth)
{
    if (ia == k_no_node || ib == k_no_node || depth > k_max_dock_nodes)
        return ia == ib;
    const DockNode& x = a.nodes[ia];
    const DockNode& y = b.nodes[ib];
    if (x.kind != y.kind)
        return false;
    if (x.kind == DockNodeKind::Tabs)
    {
        if (x.count != y.count)
            return false;
        for (uint32_t t = 0; t < x.count; ++t)
            if (x.tabs[t] != y.tabs[t])
                return false;
        return true;
    }
    return x.axis == y.axis && same_subtree(a, x.first, b, y.first, depth + 1) && same_subtree(a, x.second, b, y.second, depth + 1);
}

// Sizes, ratios and selection are ignored: a drop that only rebuilds the same tree is not a move.
bool same_arrangement(const DockLayout& a, const DockLayout& b)
{
    for (uint32_t s = 0; s < k_max_dock_surfaces; ++s)
        if (!same_subtree(a, a.roots[s], b, b.roots[s], 0))
            return false;
    if (a.float_count != b.float_count)
        return false;
    for (uint32_t f = 0; f < a.float_count; ++f)
    {
        bool found = false;
        for (uint32_t g = 0; g < b.float_count; ++g)
            found = found || a.floats[f].panel == b.floats[g].panel;
        if (!found)
            return false;
    }
    return true;
}

struct DropProbe
{
    DockReason reason = DockReason::None;
    bool changes_nothing = false;
};

DropProbe probe_drop(const DockLayout& layout, const PanelTable& panels, PanelId panel, DockTarget target, DropZone zone)
{
    DockLayout probe = layout;
    const DockResult result = dock_panel(probe, panels, panel, target, zone);
    const bool full = result.reason == DockReason::TabsFull || result.reason == DockReason::NodesFull;
    return { full ? result.reason : DockReason::None, result.reason == DockReason::NoChange || (result.applied && same_arrangement(layout, probe)) };
}

void add_guide(DropGuides& out, const DockLayout& layout, const PanelTable& panels, PanelId panel, DropZone zone, int32_t node, const Rect& rect)
{
    DockReason reason = can_dock_into(layout, panels, panel, node);
    bool here = false;
    if (reason == DockReason::None)
    {
        const DropProbe probe = probe_drop(layout, panels, panel, DockTarget{ node, out.surface }, zone);
        here = probe.changes_nothing;
        reason = probe.reason;
    }
    DropGuide& guide = out.guides[out.count++];
    guide.zone = zone;
    guide.node = node;
    guide.rect = rect;
    guide.reason = reason;
    guide.here = here;
    guide.outer = node == k_dock_root;
    guide.allowed = reason == DockReason::None;
}

// A float drawn over the pointer hides the nodes beneath it; the dragged panel's own float never does.
bool over_float(const DockLayout& layout, const SolvedLayout& solved, PanelId dragged, const Vec2f& pointer)
{
    for (uint32_t f = 0; f < layout.float_count && f < k_max_dock_floats; ++f)
        if (layout.floats[f].surface == solved.surface && layout.floats[f].panel != dragged && contains(solved.floats[f], pointer))
            return true;
    return false;
}

}

static DropGuides build_drop_guides(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer);

DropGuides drop_guides(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer)
{
    DropGuides out = build_drop_guides(layout, panels, solved, panel, pointer);
    out.surface = solved.surface;
    return out;
}

namespace
{

// Inner squares follow ImGui's scaling (a node's smaller side over 8) but are capped at 0.875 units, so they stay the compact 28 pt squares at the default text height.
float guide_half(const Rect& parent, const DockMetrics& metrics)
{
    return math::clamp(math::min(parent.size[0], parent.size[1]) / 8.0f, metrics.guide_unit * 0.5f, metrics.guide_unit * 0.875f);
}

Rect square_at(const Vec2f& centre, float half)
{
    return Rect{ Vec2f(centre[0] - half, centre[1] - half), Vec2f(2.0f * half, 2.0f * half) };
}

}

static DropGuides build_drop_guides(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer)
{
    DropGuides out;
    out.surface = solved.surface;
    const DockMetrics& metrics = solved.metrics;
    const Rect& surface = solved.surface_rect;
    const int32_t root = solved.surface < k_max_dock_surfaces ? layout.roots[solved.surface] : k_no_node;
    if (!metrics.style.guides || !contains(surface, pointer) || can_dock(panels, panel) != DockReason::None)
        return out;

    const Vec2f surface_centre(surface.min[0] + surface.size[0] * 0.5f, surface.min[1] + surface.size[1] * 0.5f);
    const float outer_half = std::trunc(metrics.guide_unit * 0.875f);
    if (root == k_no_node)
    {
        add_guide(out, layout, panels, panel, DropZone::Centre, k_dock_root, square_at(surface_centre, outer_half));
        return out;
    }

    // Outer guides sit against the window edge at its midpoint, so they read as "the edge" and never share a place with the inner cross.
    const float reach = std::trunc(metrics.guide_unit * 0.375f) + outer_half;
    add_guide(out, layout, panels, panel, DropZone::Left, k_dock_root, square_at(Vec2f(surface.min[0] + reach, surface_centre[1]), outer_half));
    add_guide(out, layout, panels, panel, DropZone::Right, k_dock_root, square_at(Vec2f(surface.min[0] + surface.size[0] - reach, surface_centre[1]), outer_half));
    add_guide(out, layout, panels, panel, DropZone::Top, k_dock_root, square_at(Vec2f(surface_centre[0], surface.min[1] + reach), outer_half));
    add_guide(out, layout, panels, panel, DropZone::Bottom, k_dock_root, square_at(Vec2f(surface_centre[0], surface.min[1] + surface.size[1] - reach), outer_half));

    int32_t hit = k_no_node;
    for (uint32_t n = 0; n < solved.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && contains(solved.nodes[n].rect, pointer))
        {
            hit = static_cast<int32_t>(n);
            break;
        }
    if (hit == k_no_node || over_float(layout, solved, panel, pointer))
        return out;

    const Rect& node = solved.nodes[hit].rect;
    const float half = std::trunc(guide_half(node, metrics));
    const Vec2f centre(std::trunc(node.min[0] + node.size[0] * 0.5f), std::trunc(node.min[1] + node.size[1] * 0.5f));
    const float step = std::trunc(half * 2.3f);
    out.centre = centre;
    out.half = half;
    out.inner_node = hit;
    add_guide(out, layout, panels, panel, DropZone::Left, hit, square_at(Vec2f(centre[0] - step, centre[1]), half));
    add_guide(out, layout, panels, panel, DropZone::Right, hit, square_at(Vec2f(centre[0] + step, centre[1]), half));
    add_guide(out, layout, panels, panel, DropZone::Top, hit, square_at(Vec2f(centre[0], centre[1] - step), half));
    add_guide(out, layout, panels, panel, DropZone::Bottom, hit, square_at(Vec2f(centre[0], centre[1] + step), half));
    add_guide(out, layout, panels, panel, DropZone::Centre, hit, square_at(centre, half));
    return out;
}

namespace
{

DropZone quadrant(const Vec2f& delta)
{
    if (math::abs(delta[0]) > math::abs(delta[1]))
        return delta[0] < 0.0f ? DropZone::Left : DropZone::Right;
    return delta[1] < 0.0f ? DropZone::Top : DropZone::Bottom;
}

}

int32_t guide_at(const DropGuides& guides, const Vec2f& pointer)
{
    for (uint32_t i = 0; i < guides.count; ++i)
        if (guides.guides[i].outer && contains(guides.guides[i].rect, pointer))
            return static_cast<int32_t>(i);

    // The inner five are read as a pie around the centre, so a diagonal move between squares keeps the nearest side instead of flickering.
    const float half = guides.half;
    const Vec2f delta = pointer - guides.centre;
    const float reach2 = delta[0] * delta[0] + delta[1] * delta[1];
    if (half > 0.0f && reach2 < (half * 2.6f) * (half * 2.6f))
        return guide_index(guides, guides.inner_node, reach2 < (half * 1.4f) * (half * 1.4f) ? DropZone::Centre : quadrant(delta));
    for (uint32_t i = 0; i < guides.count; ++i)
    {
        const DropGuide& guide = guides.guides[i];
        const float grow = guide.outer ? 0.0f : std::trunc(half * 0.3f);
        const Rect padded{ Vec2f(guide.rect.min[0] - grow, guide.rect.min[1] - grow), Vec2f(guide.rect.size[0] + 2.0f * grow, guide.rect.size[1] + 2.0f * grow) };
        if (!guide.outer && contains(padded, pointer))
            return static_cast<int32_t>(i);
    }
    return -1;
}

int32_t guide_index(const DropGuides& guides, int32_t node, DropZone zone)
{
    for (uint32_t i = 0; i < guides.count; ++i)
        if (guides.guides[i].node == node && guides.guides[i].zone == zone)
            return static_cast<int32_t>(i);
    return -1;
}

Rect guide_glyph(const Rect& guide, DropZone zone)
{
    const float pad = guide.size[0] * 0.2f;
    const Vec2f min(guide.min[0] + pad, guide.min[1] + pad);
    const Vec2f size(guide.size[0] - 2.0f * pad, guide.size[1] - 2.0f * pad);
    switch (zone)
    {
    case DropZone::Left: return Rect{ min, Vec2f(size[0] * 0.5f, size[1]) };
    case DropZone::Right: return Rect{ Vec2f(min[0] + size[0] * 0.5f, min[1]), Vec2f(size[0] * 0.5f, size[1]) };
    case DropZone::Top: return Rect{ min, Vec2f(size[0], size[1] * 0.5f) };
    case DropZone::Bottom: return Rect{ Vec2f(min[0], min[1] + size[1] * 0.5f), Vec2f(size[0], size[1] * 0.5f) };
    case DropZone::Centre: break;
    }
    return Rect{ Vec2f(min[0] + size[0] * 0.2f, min[1] + size[1] * 0.2f), Vec2f(size[0] * 0.6f, size[1] * 0.6f) };
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

static DropPlan decide_drop(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, const DropGuides& guides, PanelId panel, const Vec2f& pointer, bool float_only);

DropPlan resolve_drop(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer, bool float_only)
{
    const DropGuides guides = float_only ? DropGuides{} : drop_guides(layout, panels, solved, panel, pointer);
    return resolve_drop(layout, panels, solved, guides, panel, pointer, float_only);
}

DropPlan resolve_drop(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, const DropGuides& guides, PanelId panel, const Vec2f& pointer, bool float_only)
{
    DropPlan plan = decide_drop(layout, panels, solved, guides, panel, pointer, float_only);
    plan.surface = solved.surface;
    return plan;
}

namespace
{

float dock_size_of(const PanelTable& panels, PanelId panel)
{
    const PanelDesc* desc = find_panel(panels, panel);
    return desc != nullptr ? desc->dock_size : 0.0f;
}

// A centre drop lands under the strip, like ImGui's preview; a collapsed node has no body, so the whole node shows.
Rect centre_preview(const SolvedNode& node)
{
    return node.body.size[0] > 0.0f && node.body.size[1] > 0.0f ? node.body : node.rect;
}

}

int32_t strip_at(const DockLayout& layout, const SolvedLayout& solved, const Vec2f& pointer)
{
    for (uint32_t n = 0; n < solved.node_count && n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && contains(solved.nodes[n].strip, pointer))
            return static_cast<int32_t>(n);
    return k_no_node;
}

static DropPlan decide_drop(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, const DropGuides& guides, PanelId panel, const Vec2f& pointer, bool float_only)
{
    const int32_t source = tab_node_of(layout, panel);
    const bool floating = is_floating(layout, panel);
    if (source == k_no_node && !floating)
        return refused(DockReason::NotFound);

    if (!float_only && contains(solved.surface_rect, pointer))
    {
        const int32_t hovered = guide_at(guides, pointer);
        if (hovered >= 0)
        {
            const DropGuide& guide = guides.guides[hovered];
            DropPlan plan;
            plan.guide = hovered;
            if (guide.here)
                return plan;
            if (!guide.allowed)
            {
                plan = refused(guide.reason, guide.node, guide.zone);
                plan.guide = hovered;
                return plan;
            }
            plan.action = DropAction::Dock;
            plan.node = guide.node;
            plan.zone = guide.zone;
            if (guide.node == k_dock_root)
                plan.preview = root_preview(solved.surface_rect, guide.zone, dock_size_of(panels, panel), solved.metrics);
            else if (guide.zone == DropZone::Centre)
                plan.preview = centre_preview(solved.nodes[guide.node]);
            else
                plan.preview = edge_preview(solved.nodes[guide.node].rect, guide.zone, solved.metrics);
            plan.slot = guide.node != k_dock_root ? layout.nodes[guide.node].count : 0u;
            return plan;
        }

        if (over_float(layout, solved, panel, pointer))
            return DropPlan{};

        const int32_t strip = strip_at(layout, solved, pointer);
        if (strip == k_no_node)
            return DropPlan{};

        if (strip == source)
        {
            if (can_reorder(panels, panel) != DockReason::None)
                return DropPlan{ DropAction::None, source };
            const DockNode& node = layout.nodes[source];
            const uint32_t insertion = insertion_slot(node, solved.nodes[source], pointer[0]);
            uint32_t current = 0;
            while (current < node.count && node.tabs[current] != panel)
                ++current;
            const uint32_t slot = insertion > current ? insertion - 1u : insertion;
            DropPlan plan;
            plan.action = slot == current ? DropAction::None : DropAction::Reorder;
            plan.node = source;
            plan.slot = slot;
            if (plan.action == DropAction::Reorder)
                plan.marker = insertion_marker(node, solved.nodes[source], insertion);
            return plan;
        }

        if (const DockReason reason = can_dock_into(layout, panels, panel, strip); reason != DockReason::None)
            return refused(reason, strip, DropZone::Centre);
        const DropProbe probe = probe_drop(layout, panels, panel, DockTarget{ strip, solved.surface }, DropZone::Centre);
        if (probe.reason != DockReason::None)
            return refused(probe.reason, strip, DropZone::Centre);
        if (probe.changes_nothing)
            return DropPlan{};
        DropPlan plan;
        plan.action = DropAction::Dock;
        plan.node = strip;
        plan.zone = DropZone::Centre;
        plan.preview = centre_preview(solved.nodes[strip]);
        plan.slot = insertion_slot(layout.nodes[strip], solved.nodes[strip], pointer[0]);
        plan.marker = insertion_marker(layout.nodes[strip], solved.nodes[strip], plan.slot);
        return plan;
    }

    if (floating)
        return DropPlan{};
    const DockReason reason = can_float(panels, panel);
    if (reason != DockReason::None)
        return refused(reason);
    if (layout.float_count >= k_max_dock_floats)
        return refused(DockReason::FloatsFull);
    DropPlan plan;
    plan.action = DropAction::Float;
    plan.preview = float_preview(panels, solved, panel, pointer);
    return plan;
}

}
