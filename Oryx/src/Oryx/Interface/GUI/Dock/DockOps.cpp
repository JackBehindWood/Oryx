#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"

#include "Oryx/Core/Error.h"

namespace oryx::gui
{

namespace
{

constexpr uint32_t k_max_depth = k_max_dock_nodes + 1;

DockResult refuse(DockReason reason) { return DockResult{ false, reason }; }
DockResult accept() { return DockResult{ true, DockReason::None }; }

bool valid_node(const DockLayout& layout, int32_t node) { return node >= 0 && static_cast<uint32_t>(node) < layout.node_count && layout.node_count <= k_max_dock_nodes; }
bool is_tabs(const DockLayout& layout, int32_t node) { return valid_node(layout, node) && layout.nodes[node].kind == DockNodeKind::Tabs; }
bool is_split(const DockLayout& layout, int32_t node) { return valid_node(layout, node) && layout.nodes[node].kind == DockNodeKind::Split; }

DockReason require_flag(const PanelTable& panels, PanelId panel, uint8_t bit, DockReason denied)
{
    const PanelDesc* desc = find_panel(panels, panel);
    if (desc == nullptr)
        return DockReason::UnknownPanel;
    return has_flag(desc->flags, bit) ? DockReason::None : denied;
}

enum class Where : uint8_t
{
    Nowhere,
    InTabs,
    InFloat,
    InClosed
};

struct Location
{
    Where where = Where::Nowhere;
    int32_t node = k_no_node;
    uint32_t index = 0;
};

Location locate(const DockLayout& layout, PanelId panel)
{
    for (uint32_t n = 0; n < layout.node_count && n < k_max_dock_nodes; ++n)
    {
        const DockNode& node = layout.nodes[n];
        if (node.kind != DockNodeKind::Tabs)
            continue;
        for (uint32_t t = 0; t < node.count && t < k_max_dock_tabs; ++t)
            if (node.tabs[t] == panel)
                return { Where::InTabs, static_cast<int32_t>(n), t };
    }
    for (uint32_t f = 0; f < layout.float_count && f < k_max_dock_floats; ++f)
        if (layout.floats[f].panel == panel)
            return { Where::InFloat, k_no_node, f };
    for (uint32_t c = 0; c < layout.closed_count && c < k_max_dock_closed; ++c)
        if (layout.closed[c] == panel)
            return { Where::InClosed, k_no_node, c };
    return {};
}

int32_t parent_of(const DockLayout& layout, int32_t node)
{
    for (uint32_t n = 0; n < layout.node_count; ++n)
    {
        const DockNode& candidate = layout.nodes[n];
        if (candidate.kind == DockNodeKind::Split && (candidate.first == node || candidate.second == node))
            return static_cast<int32_t>(n);
    }
    return k_no_node;
}

void replace_child(DockLayout& layout, int32_t old_node, int32_t new_node)
{
    const int32_t parent = parent_of(layout, old_node);
    if (parent >= 0)
    {
        DockNode& split = layout.nodes[parent];
        (split.first == old_node ? split.first : split.second) = new_node;
        return;
    }
    for (int32_t& root : layout.roots)
        if (root == old_node)
            root = new_node;
}

int32_t alloc_node(DockLayout& layout)
{
    if (layout.node_count >= k_max_dock_nodes)
        return k_no_node;
    layout.nodes[layout.node_count] = DockNode{};
    return static_cast<int32_t>(layout.node_count++);
}

void remove_tab(DockLayout& layout, int32_t node, uint32_t index)
{
    DockNode& n = layout.nodes[node];
    for (uint32_t i = index; i + 1 < n.count; ++i)
        n.tabs[i] = n.tabs[i + 1];
    --n.count;
    n.tabs[n.count] = PanelId{};
    if (index < n.selected)
        --n.selected;
    if (n.count > 0 && n.selected >= n.count)
        n.selected = static_cast<uint8_t>(n.count - 1);
}

void remove_float(DockLayout& layout, uint32_t index)
{
    for (uint32_t i = index; i + 1 < layout.float_count; ++i)
        layout.floats[i] = layout.floats[i + 1];
    --layout.float_count;
    layout.floats[layout.float_count] = DockFloat{};
}

void remove_closed(DockLayout& layout, uint32_t index)
{
    for (uint32_t i = index; i + 1 < layout.closed_count; ++i)
        layout.closed[i] = layout.closed[i + 1];
    --layout.closed_count;
    layout.closed[layout.closed_count] = PanelId{};
}

void remove_home(DockLayout& layout, PanelId panel)
{
    for (uint32_t i = 0; i < layout.home_count; ++i)
    {
        if (layout.homes[i].panel != panel)
            continue;
        for (uint32_t j = i; j + 1 < layout.home_count; ++j)
            layout.homes[j] = layout.homes[j + 1];
        --layout.home_count;
        layout.homes[layout.home_count] = DockHome{};
        return;
    }
}

void push_home(DockLayout& layout, const DockHome& home)
{
    remove_home(layout, home.panel);
    if (layout.home_count >= k_max_dock_homes)
    {
        for (uint32_t i = 0; i + 1 < layout.home_count; ++i)
            layout.homes[i] = layout.homes[i + 1];
        --layout.home_count;
    }
    layout.homes[layout.home_count++] = home;
}

void detach(DockLayout& layout, PanelId panel, const Location& at)
{
    switch (at.where)
    {
    case Where::InTabs: remove_tab(layout, at.node, at.index); break;
    case Where::InFloat: remove_float(layout, at.index); break;
    case Where::InClosed: remove_closed(layout, at.index); break;
    case Where::Nowhere: break;
    }
    remove_home(layout, panel);
}

PanelId first_panel(const DockLayout& layout, int32_t node)
{
    for (uint32_t depth = 0; depth < k_max_depth && valid_node(layout, node); ++depth)
    {
        const DockNode& n = layout.nodes[node];
        if (n.kind == DockNodeKind::Tabs)
            return n.count > 0 ? n.tabs[0] : PanelId{};
        if (n.kind != DockNodeKind::Split)
            break;
        node = n.first;
    }
    return PanelId{};
}

// Never mutates on failure, so callers may try another target afterwards.
DockReason insert_panel(DockLayout& layout, PanelId panel, int32_t target, DropZone zone, uint8_t surface = 0)
{
    if (target == k_no_node)
    {
        const int32_t root = alloc_node(layout);
        if (root < 0)
            return DockReason::NodesFull;
        DockNode& n = layout.nodes[root];
        n.kind = DockNodeKind::Tabs;
        n.count = 1;
        n.tabs[0] = panel;
        layout.roots[surface] = root;
        return DockReason::None;
    }

    if (zone == DropZone::Centre)
    {
        if (!is_tabs(layout, target))
            return DockReason::TargetInvalid;
        DockNode& n = layout.nodes[target];
        if (n.count >= k_max_dock_tabs)
            return DockReason::TabsFull;
        n.tabs[n.count] = panel;
        n.selected = n.count;
        ++n.count;
        n.collapsed = 0;
        return DockReason::None;
    }

    if (!valid_node(layout, target))
        return DockReason::TargetInvalid;
    if (layout.node_count + 2 > k_max_dock_nodes)
        return DockReason::NodesFull;

    const int32_t split = alloc_node(layout);
    const int32_t leaf = alloc_node(layout);
    DockNode& l = layout.nodes[leaf];
    l.kind = DockNodeKind::Tabs;
    l.count = 1;
    l.tabs[0] = panel;

    replace_child(layout, target, split);
    init_split(layout.nodes[split], zone, leaf, target);
    return DockReason::None;
}

void fix_new_split(DockLayout& layout, PanelId panel, DropZone zone, float points)
{
    if (!(points > 0.0f))
        return;
    const Location at = locate(layout, panel);
    const int32_t parent = at.where == Where::InTabs ? parent_of(layout, at.node) : k_no_node;
    if (parent < 0)
        return;
    DockNode& split = layout.nodes[parent];
    split.mode = (zone == DropZone::Left || zone == DropZone::Top) ? DockSizeMode::FixedFirst : DockSizeMode::FixedSecond;
    split.points = points;
}

// Never mutates on failure; the sibling-open check of a home is the caller's.
DockReason place_by_hints(DockLayout& layout, const PanelDesc& desc, uint8_t surface = 0)
{
    if (is_valid(desc.dock_tabbed_with))
    {
        const Location with = locate(layout, desc.dock_tabbed_with);
        if (with.where == Where::InTabs && insert_panel(layout, desc.id, with.node, DropZone::Centre) == DockReason::None)
            return DockReason::None;
    }
    const DropZone side = desc.dock_side == DropZone::Centre ? DropZone::Right : desc.dock_side;
    const Location near = locate(layout, desc.dock_near);
    int32_t target = k_no_node;
    if (is_valid(desc.dock_near) && near.where == Where::InTabs)
        target = near.node;
    else if (is_valid(desc.dock_near) || desc.dock_size > 0.0f)
        target = layout.roots[surface];
    if (target == k_no_node)
        return DockReason::TargetInvalid;
    const DockReason placed = insert_panel(layout, desc.id, target, side);
    if (placed == DockReason::None)
        fix_new_split(layout, desc.id, side, desc.dock_size);
    return placed;
}

DockHome make_home(const DockLayout& layout, PanelId panel, const Location& at)
{
    DockHome home;
    home.panel = panel;
    if (at.where != Where::InTabs)
        return home;
    const DockNode& n = layout.nodes[at.node];
    if (n.count > 1)
    {
        home.sibling = n.tabs[at.index > 0 ? at.index - 1 : at.index + 1];
        return home;
    }
    const int32_t parent = parent_of(layout, at.node);
    if (parent < 0)
        return home;
    const DockNode& p = layout.nodes[parent];
    const bool is_first = p.first == at.node;
    home.sibling = first_panel(layout, is_first ? p.second : p.first);
    if (p.axis == DockAxis::Horizontal)
        home.zone = is_first ? DropZone::Left : DropZone::Right;
    else
        home.zone = is_first ? DropZone::Top : DropZone::Bottom;
    return home;
}

bool side_resizable(const DockLayout& layout, const PanelTable& panels, int32_t node, uint32_t depth)
{
    if (!valid_node(layout, node) || depth > k_max_depth)
        return false;
    const DockNode& n = layout.nodes[node];
    if (n.kind == DockNodeKind::Tabs)
    {
        for (uint32_t t = 0; t < n.count && t < k_max_dock_tabs; ++t)
            if (can_resize(panels, n.tabs[t]) == DockReason::None)
                return true;
        return false;
    }
    if (n.kind == DockNodeKind::Split)
        return side_resizable(layout, panels, n.first, depth + 1) || side_resizable(layout, panels, n.second, depth + 1);
    return false;
}

bool all_collapsed_at(const DockLayout& layout, int32_t node, uint32_t depth)
{
    if (!valid_node(layout, node) || depth > k_max_depth)
        return false;
    const DockNode& n = layout.nodes[node];
    if (n.kind == DockNodeKind::Tabs)
        return n.collapsed != 0;
    return n.kind == DockNodeKind::Split && all_collapsed_at(layout, n.first, depth + 1) && all_collapsed_at(layout, n.second, depth + 1);
}

int32_t root_of(const DockLayout& layout, int32_t node)
{
    for (uint32_t depth = 0; depth < k_max_depth; ++depth)
    {
        const int32_t parent = parent_of(layout, node);
        if (parent < 0)
            return node;
        node = parent;
    }
    return node;
}

bool holds_viewport(const DockLayout& layout, const PanelTable& panels, int32_t node)
{
    const DockNode& n = layout.nodes[node];
    for (uint32_t t = 0; t < n.count && t < k_max_dock_tabs; ++t)
    {
        const PanelDesc* desc = find_panel(panels, n.tabs[t]);
        if (desc != nullptr && desc->kind == PanelKind::Viewport)
            return true;
    }
    return false;
}

bool all_may_collapse(const DockNode& n, const PanelTable& panels)
{
    for (uint32_t t = 0; t < n.count && t < k_max_dock_tabs; ++t)
    {
        const PanelDesc* desc = find_panel(panels, n.tabs[t]);
        if (desc != nullptr && !has_flag(desc->flags, panel_flag::collapse))
            return false;
    }
    return true;
}

int32_t prune(DockLayout& layout, int32_t node, uint32_t depth)
{
    if (!valid_node(layout, node) || depth > k_max_depth)
        return k_no_node;
    DockNode& n = layout.nodes[node];
    if (n.kind == DockNodeKind::Tabs)
    {
        if (n.count > k_max_dock_tabs)
            n.count = static_cast<uint8_t>(k_max_dock_tabs);
        return n.count == 0 ? k_no_node : node;
    }
    if (n.kind != DockNodeKind::Split)
        return k_no_node;
    const int32_t a = prune(layout, n.first, depth + 1);
    const int32_t b = prune(layout, n.second, depth + 1);
    if (a < 0)
        return b;
    if (b < 0)
        return a;
    n.first = a;
    n.second = b;
    return node;
}

int32_t copy_node(const DockLayout& src, int32_t index, DockLayout& out, uint32_t depth)
{
    if (depth > k_max_depth || out.node_count >= k_max_dock_nodes)
        return k_no_node;
    const int32_t dst = static_cast<int32_t>(out.node_count++);
    const DockNode& s = src.nodes[index];
    DockNode d;
    d.kind = s.kind;
    if (s.kind == DockNodeKind::Tabs)
    {
        d.count = static_cast<uint8_t>(math::min<uint32_t>(s.count, k_max_dock_tabs));
        for (uint32_t t = 0; t < d.count; ++t)
            d.tabs[t] = s.tabs[t];
        d.selected = d.count == 0 ? 0 : static_cast<uint8_t>(math::min<uint32_t>(s.selected, d.count - 1u));
        d.collapsed = s.collapsed != 0 ? 1 : 0;
        out.nodes[dst] = d;
        return dst;
    }
    d.axis = static_cast<uint8_t>(s.axis) <= static_cast<uint8_t>(DockAxis::Vertical) ? s.axis : DockAxis::Horizontal;
    d.mode = static_cast<uint8_t>(s.mode) <= static_cast<uint8_t>(DockSizeMode::FixedSecond) ? s.mode : DockSizeMode::Ratio;
    d.ratio = std::isfinite(s.ratio) ? math::clamp(s.ratio, k_dock_min_ratio, 1.0f - k_dock_min_ratio) : 0.5f;
    d.points = std::isfinite(s.points) ? math::max(s.points, 0.0f) : 0.0f;
    out.nodes[dst] = d;
    const int32_t a = copy_node(src, s.first, out, depth + 1);
    const int32_t b = copy_node(src, s.second, out, depth + 1);
    out.nodes[dst].first = a;
    out.nodes[dst].second = b;
    return dst;
}

bool same_node(const DockNode& a, const DockNode& b)
{
    if (a.kind != b.kind)
        return false;
    if (a.kind == DockNodeKind::Tabs)
    {
        if (a.count != b.count || a.selected != b.selected || a.collapsed != b.collapsed)
            return false;
        for (uint32_t t = 0; t < a.count && t < k_max_dock_tabs; ++t)
            if (a.tabs[t] != b.tabs[t])
                return false;
        return true;
    }
    if (a.kind == DockNodeKind::Split)
        return a.axis == b.axis && a.mode == b.mode && std::bit_cast<uint32_t>(a.ratio) == std::bit_cast<uint32_t>(b.ratio) && std::bit_cast<uint32_t>(a.points) == std::bit_cast<uint32_t>(b.points) && a.first == b.first && a.second == b.second;
    return true;
}

bool same_bits(float a, float b) { return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b); }

void append_float(std::string& out, float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", static_cast<double>(value));
    out += buffer;
}

void append_uint(std::string& out, uint32_t value) { out += std::to_string(value); }

void append_panel(std::string& out, const PanelTable* panels, PanelId id)
{
    if (!is_valid(id))
    {
        out += "-";
        return;
    }
    if (panels != nullptr)
    {
        const PanelDesc* desc = find_panel(*panels, id);
        if (desc != nullptr && desc->name[0] != '\0')
        {
            out += desc->name;
            return;
        }
    }
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "#%08x", id.hash);
    out += buffer;
}

const char* zone_name(DropZone zone)
{
    switch (zone)
    {
    case DropZone::Centre: return "centre";
    case DropZone::Left: return "left";
    case DropZone::Right: return "right";
    case DropZone::Top: return "top";
    case DropZone::Bottom: return "bottom";
    }
    return "?";
}

void dump_node(std::string& out, const DockLayout& layout, const PanelTable* panels, int32_t node, uint32_t indent, uint32_t depth)
{
    out.append(indent * 2, ' ');
    if (!valid_node(layout, node) || depth > k_max_depth)
    {
        out += "invalid node\n";
        return;
    }
    const DockNode& n = layout.nodes[node];
    if (n.kind == DockNodeKind::Tabs)
    {
        out += "tabs [";
        for (uint32_t t = 0; t < n.count && t < k_max_dock_tabs; ++t)
        {
            if (t > 0)
                out += " ";
            append_panel(out, panels, n.tabs[t]);
            if (t == n.selected)
                out += "*";
        }
        out += n.collapsed != 0 ? "] collapsed\n" : "]\n";
        return;
    }
    if (n.kind != DockNodeKind::Split)
    {
        out += "empty\n";
        return;
    }
    out += n.axis == DockAxis::Horizontal ? "split h " : "split v ";
    if (n.mode == DockSizeMode::Ratio)
    {
        out += "ratio ";
        append_float(out, n.ratio);
    }
    else
    {
        out += n.mode == DockSizeMode::FixedFirst ? "first " : "second ";
        append_float(out, n.points);
    }
    out += "\n";
    dump_node(out, layout, panels, n.first, indent + 1, depth + 1);
    dump_node(out, layout, panels, n.second, indent + 1, depth + 1);
}

std::string dump_impl(const DockLayout& layout, const PanelTable* panels)
{
    std::string out = "dock v";
    append_uint(out, layout.version);
    out += "\n";
    for (uint32_t s = 0; s < k_max_dock_surfaces; ++s)
    {
        if (layout.roots[s] == k_no_node)
            continue;
        out += "surface ";
        append_uint(out, s);
        out += "\n";
        dump_node(out, layout, panels, layout.roots[s], 1, 0);
    }
    for (uint32_t f = 0; f < layout.float_count && f < k_max_dock_floats; ++f)
    {
        const DockFloat& fl = layout.floats[f];
        out += "float ";
        append_panel(out, panels, fl.panel);
        out += " surface ";
        append_uint(out, fl.surface);
        out += " rect ";
        append_float(out, fl.rect.min[0]);
        out += " ";
        append_float(out, fl.rect.min[1]);
        out += " ";
        append_float(out, fl.rect.size[0]);
        out += " ";
        append_float(out, fl.rect.size[1]);
        out += "\n";
    }
    for (uint32_t h = 0; h < layout.home_count && h < k_max_dock_homes; ++h)
    {
        out += "home ";
        append_panel(out, panels, layout.homes[h].panel);
        out += " sibling ";
        append_panel(out, panels, layout.homes[h].sibling);
        out += " ";
        out += zone_name(layout.homes[h].zone);
        out += "\n";
    }
    for (uint32_t c = 0; c < layout.closed_count && c < k_max_dock_closed; ++c)
    {
        out += "closed ";
        append_panel(out, panels, layout.closed[c]);
        out += "\n";
    }
    return out;
}

std::vector<std::string_view> split_lines(const std::string& text)
{
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < text.size())
    {
        const size_t end = text.find('\n', start);
        if (end == std::string::npos)
        {
            lines.emplace_back(text.data() + start, text.size() - start);
            break;
        }
        lines.emplace_back(text.data() + start, end - start);
        start = end + 1;
    }
    return lines;
}

[[noreturn]] void fail(const std::string& detail) { throw Error("Invalid dock layout: " + detail); }

void validate_node(const DockLayout& layout, int32_t node, bool* referenced, uint32_t depth)
{
    if (node < 0 || static_cast<uint32_t>(node) >= layout.node_count)
        fail("node index " + std::to_string(node) + " out of range");
    if (depth > layout.node_count)
        fail("tree deeper than its node count (cycle)");
    if (referenced[node])
        fail("node " + std::to_string(node) + " reachable twice (shared or cyclic)");
    referenced[node] = true;

    const DockNode& n = layout.nodes[node];
    if (n.kind == DockNodeKind::Tabs)
    {
        if (n.count < 1 || n.count > k_max_dock_tabs)
            fail("tabs node " + std::to_string(node) + " has count " + std::to_string(n.count));
        if (n.selected >= n.count)
            fail("tabs node " + std::to_string(node) + " selected index out of range");
        if (n.collapsed > 1)
            fail("tabs node " + std::to_string(node) + " collapsed flag out of range");
        for (uint32_t t = 0; t < n.count; ++t)
            if (!is_valid(n.tabs[t]))
                fail("tabs node " + std::to_string(node) + " holds the invalid panel id");
        return;
    }
    if (n.kind != DockNodeKind::Split)
        fail("node " + std::to_string(node) + " is empty or has an unknown kind");
    if (static_cast<uint8_t>(n.axis) > static_cast<uint8_t>(DockAxis::Vertical))
        fail("split " + std::to_string(node) + " axis out of range");
    if (static_cast<uint8_t>(n.mode) > static_cast<uint8_t>(DockSizeMode::FixedSecond))
        fail("split " + std::to_string(node) + " size mode out of range");
    if (!std::isfinite(n.ratio) || !(n.ratio > 0.0f) || !(n.ratio < 1.0f))
        fail("split " + std::to_string(node) + " ratio is not in (0, 1)");
    if (!std::isfinite(n.points) || n.points < 0.0f)
        fail("split " + std::to_string(node) + " points are negative or not finite");
    validate_node(layout, n.first, referenced, depth + 1);
    validate_node(layout, n.second, referenced, depth + 1);
}

}

const char* to_string(DockReason reason)
{
    switch (reason)
    {
    case DockReason::None: return "none";
    case DockReason::UnknownPanel: return "unknown panel";
    case DockReason::NotPermittedReorder: return "panel may not reorder within its host";
    case DockReason::NotPermittedDock: return "panel may not dock elsewhere";
    case DockReason::NotPermittedFloat: return "panel may not float";
    case DockReason::NotPermittedResize: return "panel may not resize";
    case DockReason::NotPermittedCollapse: return "panel may not collapse";
    case DockReason::NotPermittedClose: return "panel may not close";
    case DockReason::NoChange: return "no change";
    case DockReason::TargetInvalid: return "invalid target";
    case DockReason::BadArgument: return "bad argument";
    case DockReason::NotFound: return "panel not found in its host";
    case DockReason::TabsFull: return "tab stack full";
    case DockReason::NodesFull: return "node pool full";
    case DockReason::FloatsFull: return "float pool full";
    case DockReason::PoolFull: return "record pool full";
    case DockReason::AlreadyOpen: return "panel already open";
    case DockReason::AlreadyClosed: return "panel already closed";
    case DockReason::NotPermittedTarget: return "panel may not dock there";
    case DockReason::Collapsed: return "node is collapsed";
    }
    return "?";
}

const char* describe(DockReason reason)
{
    switch (reason)
    {
    case DockReason::None: return "";
    case DockReason::NotPermittedReorder: return "This panel can't be reordered";
    case DockReason::NotPermittedDock: return "This panel can't be docked";
    case DockReason::NotPermittedFloat: return "This panel can't be floated";
    case DockReason::NotPermittedTarget: return "This panel can't dock here";
    case DockReason::TabsFull: return "This tab group is full";
    case DockReason::NodesFull: return "No room for another split";
    case DockReason::FloatsFull: return "Too many floating windows";
    case DockReason::Collapsed: return "Keep one panel expanded";
    case DockReason::TargetInvalid: return "Nothing to dock to here";
    default: break;
    }
    return "Can't drop here";
}

bool is_open(const DockLayout& layout, PanelId panel)
{
    const Location at = locate(layout, panel);
    return at.where == Where::InTabs || at.where == Where::InFloat;
}

bool all_collapsed(const DockLayout& layout, int32_t node)
{
    return all_collapsed_at(layout, node, 0);
}

bool can_resize_split(const DockLayout& layout, const PanelTable& panels, int32_t node)
{
    if (!is_split(layout, node))
        return false;
    const DockNode& n = layout.nodes[node];
    if (n.axis == DockAxis::Vertical && (all_collapsed(layout, n.first) || all_collapsed(layout, n.second)))
        return false;
    return side_resizable(layout, panels, n.first, 0) && side_resizable(layout, panels, n.second, 0);
}

DockReason can_dock_into(const DockLayout& layout, const PanelTable& panels, PanelId panel, int32_t target)
{
    if (const DockReason reason = can_dock(panels, panel); reason != DockReason::None)
        return reason;
    const PanelDesc* desc = find_panel(panels, panel);
    const bool in_node = target != k_dock_root && is_tabs(layout, target);
    if (desc->dock_only_count > 0 && !in_node)
        return DockReason::NotPermittedTarget;
    if (!in_node)
        return DockReason::None;
    const DockNode& node = layout.nodes[target];
    bool allowed = desc->dock_only_count == 0;
    for (uint32_t t = 0; t < node.count && t < k_max_dock_tabs; ++t)
    {
        for (uint32_t r = 0; r < desc->dock_never_count; ++r)
            if (node.tabs[t] == desc->dock_never[r])
                return DockReason::NotPermittedTarget;
        for (uint32_t r = 0; r < desc->dock_only_count; ++r)
            allowed = allowed || node.tabs[t] == desc->dock_only[r];
    }
    return allowed ? DockReason::None : DockReason::NotPermittedTarget;
}

DockReason can_reorder(const PanelTable& panels, PanelId panel) { return require_flag(panels, panel, panel_flag::reorder_in_host, DockReason::NotPermittedReorder); }
DockReason can_dock(const PanelTable& panels, PanelId panel) { return require_flag(panels, panel, panel_flag::dock_elsewhere, DockReason::NotPermittedDock); }
DockReason can_float(const PanelTable& panels, PanelId panel) { return require_flag(panels, panel, panel_flag::tear_off, DockReason::NotPermittedFloat); }
DockReason can_resize(const PanelTable& panels, PanelId panel) { return require_flag(panels, panel, panel_flag::resize, DockReason::NotPermittedResize); }
DockReason can_collapse(const PanelTable& panels, PanelId panel) { return require_flag(panels, panel, panel_flag::collapse, DockReason::NotPermittedCollapse); }
DockReason can_close(const PanelTable& panels, PanelId panel) { return require_flag(panels, panel, panel_flag::close, DockReason::NotPermittedClose); }

DockResult dock_panel(DockLayout& layout, const PanelTable& panels, PanelId panel, DockTarget at_target, DropZone zone)
{
    const int32_t target = at_target.node;
    const uint8_t surface = at_target.surface;
    if (surface >= k_max_dock_surfaces)
        return refuse(DockReason::TargetInvalid);
    if (find_panel(panels, panel) == nullptr)
        return refuse(DockReason::UnknownPanel);

    const int32_t resolved = target == k_dock_root ? layout.roots[surface] : target;
    if (target != k_dock_root && !is_tabs(layout, target))
        return refuse(DockReason::TargetInvalid);
    if (target == k_dock_root && resolved != k_no_node && zone == DropZone::Centre && !is_tabs(layout, resolved))
        return refuse(DockReason::TargetInvalid);

    const Location at = locate(layout, panel);
    const bool same_host = at.where == Where::InTabs && at.node == resolved;

    if (same_host && zone == DropZone::Centre)
    {
        if (const DockReason reason = can_reorder(panels, panel); reason != DockReason::None)
            return refuse(reason);
        if (at.index + 1 == layout.nodes[resolved].count)
            return refuse(DockReason::NoChange);
        DockLayout work = layout;
        remove_tab(work, resolved, at.index);
        work.nodes[resolved].tabs[work.nodes[resolved].count] = panel;
        work.nodes[resolved].selected = work.nodes[resolved].count;
        ++work.nodes[resolved].count;
        layout = work;
        return accept();
    }
    if (same_host && layout.nodes[resolved].count == 1)
        return refuse(DockReason::NoChange);

    if (const DockReason reason = can_dock_into(layout, panels, panel, target == k_dock_root && zone == DropZone::Centre && is_tabs(layout, resolved) ? resolved : target); reason != DockReason::None)
        return refuse(reason);

    // Node indices shift once the emptied source host is pruned, so the target is re-found through a panel that stays put.
    PanelId anchor;
    if (target != k_dock_root)
        for (uint32_t t = 0; t < layout.nodes[resolved].count && !is_valid(anchor); ++t)
            if (layout.nodes[resolved].tabs[t] != panel)
                anchor = layout.nodes[resolved].tabs[t];

    DockLayout work = layout;
    detach(work, panel, at);
    normalize(work, panels);
    const int32_t node = target == k_dock_root ? work.roots[surface] : locate(work, anchor).node;
    if (target != k_dock_root && node == k_no_node)
        return refuse(DockReason::TargetInvalid);
    if (const DockReason reason = insert_panel(work, panel, node, zone, surface); reason != DockReason::None)
        return refuse(reason);
    if (target == k_dock_root && zone != DropZone::Centre)
        fix_new_split(work, panel, zone, find_panel(panels, panel)->dock_size);
    normalize(work, panels);
    layout = work;
    return accept();
}

DockResult float_panel(DockLayout& layout, const PanelTable& panels, PanelId panel, const Rect& rect, uint8_t surface)
{
    if (surface >= k_max_dock_surfaces)
        return refuse(DockReason::BadArgument);
    if (find_panel(panels, panel) == nullptr)
        return refuse(DockReason::UnknownPanel);
    if (const DockReason reason = can_float(panels, panel); reason != DockReason::None)
        return refuse(reason);
    if (!std::isfinite(rect.min[0]) || !std::isfinite(rect.min[1]) || !std::isfinite(rect.size[0]) || !std::isfinite(rect.size[1]) || !(rect.size[0] > 0.0f) || !(rect.size[1] > 0.0f))
        return refuse(DockReason::BadArgument);

    const Location at = locate(layout, panel);
    if (at.where == Where::InFloat)
    {
        if (layout.floats[at.index].rect == rect)
            return refuse(DockReason::NoChange);
        layout.floats[at.index].rect = rect;
        return accept();
    }
    if (layout.float_count >= k_max_dock_floats)
        return refuse(DockReason::FloatsFull);

    DockLayout work = layout;
    detach(work, panel, at);
    DockFloat& slot = work.floats[work.float_count++];
    slot.panel = panel;
    slot.surface = surface;
    slot.rect = rect;
    normalize(work, panels);
    layout = work;
    return accept();
}

DockResult close_panel(DockLayout& layout, const PanelTable& panels, PanelId panel)
{
    if (find_panel(panels, panel) == nullptr)
        return refuse(DockReason::UnknownPanel);
    if (const DockReason reason = can_close(panels, panel); reason != DockReason::None)
        return refuse(reason);
    const Location at = locate(layout, panel);
    if (at.where == Where::Nowhere || at.where == Where::InClosed)
        return refuse(DockReason::AlreadyClosed);
    if (layout.closed_count >= k_max_dock_closed)
        return refuse(DockReason::PoolFull);

    DockLayout work = layout;
    const DockHome home = make_home(work, panel, at);
    detach(work, panel, at);
    push_home(work, home);
    work.closed[work.closed_count++] = panel;
    normalize(work, panels);
    layout = work;
    return accept();
}

DockResult open_panel(DockLayout& layout, const PanelTable& panels, PanelId panel, uint8_t surface)
{
    if (surface >= k_max_dock_surfaces)
        return refuse(DockReason::BadArgument);
    if (find_panel(panels, panel) == nullptr)
        return refuse(DockReason::UnknownPanel);
    const Location at = locate(layout, panel);
    if (at.where == Where::InTabs || at.where == Where::InFloat)
        return refuse(DockReason::AlreadyOpen);

    DockLayout work = layout;
    if (at.where == Where::InClosed)
        remove_closed(work, at.index);

    const PanelDesc& desc = *find_panel(panels, panel);
    const bool tab_anchor_open = is_valid(desc.dock_tabbed_with) && locate(work, desc.dock_tabbed_with).where == Where::InTabs;
    DockReason placed = DockReason::TargetInvalid;
    for (uint32_t h = 0; h < work.home_count && placed != DockReason::None; ++h)
    {
        const DockHome& home = work.homes[h];
        // A split home loses to the tab stack the panel asks to join, so a family stays together.
        if (home.panel != panel || !is_valid(home.sibling) || (tab_anchor_open && home.zone != DropZone::Centre))
            continue;
        const Location sibling = locate(work, home.sibling);
        if (sibling.where == Where::InTabs)
        {
            placed = insert_panel(work, panel, sibling.node, home.zone);
            if (placed == DockReason::None && home.zone != DropZone::Centre)
                fix_new_split(work, panel, home.zone, desc.dock_size);
        }
    }

    if (placed != DockReason::None && work.roots[surface] != k_no_node)
        placed = place_by_hints(work, desc, surface);

    if (placed != DockReason::None)
    {
        const int32_t root = work.roots[surface];
        if (root == k_no_node)
        {
            placed = insert_panel(work, panel, k_no_node, DropZone::Centre, surface);
        }
        else
        {
            placed = DockReason::TabsFull;
            for (uint32_t n = 0; n < work.node_count && placed != DockReason::None; ++n)
                if (work.nodes[n].kind == DockNodeKind::Tabs && root_of(work, static_cast<int32_t>(n)) == root && work.nodes[n].count < k_max_dock_tabs && !holds_viewport(work, panels, static_cast<int32_t>(n)))
                    placed = insert_panel(work, panel, static_cast<int32_t>(n), DropZone::Centre);
            if (placed != DockReason::None)
                placed = insert_panel(work, panel, root, DropZone::Right);
            for (uint32_t n = 0; n < work.node_count && placed != DockReason::None; ++n)
                if (work.nodes[n].kind == DockNodeKind::Tabs && root_of(work, static_cast<int32_t>(n)) == root && work.nodes[n].count < k_max_dock_tabs)
                    placed = insert_panel(work, panel, static_cast<int32_t>(n), DropZone::Centre);
        }
    }
    if (placed != DockReason::None)
        return refuse(placed);

    normalize(work, panels);
    layout = work;
    return accept();
}

void init_split(DockNode& split, DropZone zone, int32_t leaf, int32_t target, float points)
{
    const bool leaf_first = zone == DropZone::Left || zone == DropZone::Top;
    split.kind = DockNodeKind::Split;
    split.axis = (zone == DropZone::Left || zone == DropZone::Right) ? DockAxis::Horizontal : DockAxis::Vertical;
    split.mode = points > 0.0f ? (leaf_first ? DockSizeMode::FixedFirst : DockSizeMode::FixedSecond) : DockSizeMode::Ratio;
    split.ratio = leaf_first ? k_dock_new_ratio : 1.0f - k_dock_new_ratio;
    split.points = points > 0.0f ? points : split.points;
    split.first = leaf_first ? leaf : target;
    split.second = leaf_first ? target : leaf;
}

bool group_open(const DockLayout& layout, const PanelTable& panels, PanelId group)
{
    for (uint32_t i = 0; i < panels.count && i < k_max_panels; ++i)
        if (panels.descs[i].group == group && is_open(layout, panels.descs[i].id))
            return true;
    return false;
}

DockResult set_group_open(DockLayout& layout, const PanelTable& panels, PanelId group, bool open)
{
    if (!is_valid(group))
        return refuse(DockReason::BadArgument);
    DockLayout work = layout;
    bool any = false;
    bool found = false;
    for (uint32_t i = 0; i < panels.count && i < k_max_panels; ++i)
    {
        const PanelDesc& desc = panels.descs[i];
        if (desc.group != group)
            continue;
        found = true;
        any = (open ? open_panel(work, panels, desc.id) : close_panel(work, panels, desc.id)).applied || any;
    }
    if (!found)
        return refuse(DockReason::NotFound);
    if (!any)
        return refuse(DockReason::NoChange);
    layout = work;
    return accept();
}

DockResult reorder_tab(DockLayout& layout, const PanelTable& panels, PanelId panel, uint32_t index)
{
    if (find_panel(panels, panel) == nullptr)
        return refuse(DockReason::UnknownPanel);
    if (const DockReason reason = can_reorder(panels, panel); reason != DockReason::None)
        return refuse(reason);
    const Location at = locate(layout, panel);
    if (at.where != Where::InTabs)
        return refuse(DockReason::NotFound);
    const DockNode& node = layout.nodes[at.node];
    if (index >= node.count)
        return refuse(DockReason::BadArgument);
    if (index == at.index)
        return refuse(DockReason::NoChange);

    DockLayout work = layout;
    DockNode& n = work.nodes[at.node];
    const PanelId selected = n.tabs[n.selected];
    if (index > at.index)
        for (uint32_t i = at.index; i < index; ++i)
            n.tabs[i] = n.tabs[i + 1];
    else
        for (uint32_t i = at.index; i > index; --i)
            n.tabs[i] = n.tabs[i - 1];
    n.tabs[index] = panel;
    for (uint32_t i = 0; i < n.count; ++i)
        if (n.tabs[i] == selected)
            n.selected = static_cast<uint8_t>(i);
    layout = work;
    return accept();
}

DockResult select_tab(DockLayout& layout, int32_t node, uint32_t index)
{
    if (!is_tabs(layout, node))
        return refuse(DockReason::TargetInvalid);
    DockNode& n = layout.nodes[node];
    if (index >= n.count)
        return refuse(DockReason::BadArgument);
    if (n.selected == index)
        return refuse(DockReason::NoChange);
    n.selected = static_cast<uint8_t>(index);
    return accept();
}

DockResult set_collapsed(DockLayout& layout, const PanelTable& panels, int32_t node, bool collapsed)
{
    if (!is_tabs(layout, node))
        return refuse(DockReason::TargetInvalid);
    DockNode& n = layout.nodes[node];
    for (uint32_t t = 0; collapsed && t < n.count && t < k_max_dock_tabs; ++t)
        if (const DockReason reason = can_collapse(panels, n.tabs[t]); reason != DockReason::None)
            return refuse(reason);
    const uint8_t value = collapsed ? 1 : 0;
    if (n.collapsed == value)
        return refuse(DockReason::NoChange);
    n.collapsed = value;
    if (collapsed && all_collapsed(layout, root_of(layout, node)))
    {
        n.collapsed = 0;
        return refuse(DockReason::Collapsed);
    }
    return accept();
}

DockResult set_split(DockLayout& layout, const PanelTable& panels, int32_t node, DockSizeMode mode, float ratio, float points)
{
    if (!is_split(layout, node))
        return refuse(DockReason::TargetInvalid);
    DockNode& n = layout.nodes[node];
    if (!side_resizable(layout, panels, n.first, 0) || !side_resizable(layout, panels, n.second, 0))
        return refuse(DockReason::NotPermittedResize);
    if (n.axis == DockAxis::Vertical && (all_collapsed(layout, n.first) || all_collapsed(layout, n.second)))
        return refuse(DockReason::Collapsed);
    if (!std::isfinite(ratio) || !std::isfinite(points))
        return refuse(DockReason::BadArgument);

    const float clamped_ratio = math::clamp(ratio, k_dock_min_ratio, 1.0f - k_dock_min_ratio);
    const float clamped_points = math::max(points, 0.0f);
    if (n.mode == mode && same_bits(n.ratio, clamped_ratio) && same_bits(n.points, clamped_points))
        return refuse(DockReason::NoChange);
    n.mode = mode;
    n.ratio = clamped_ratio;
    n.points = clamped_points;
    return accept();
}

void normalize(DockLayout& layout)
{
    layout.node_count = math::min(layout.node_count, k_max_dock_nodes);
    layout.float_count = math::min(layout.float_count, k_max_dock_floats);
    layout.home_count = math::min(layout.home_count, k_max_dock_homes);
    layout.closed_count = math::min(layout.closed_count, k_max_dock_closed);

    DockLayout out;
    out.version = layout.version;
    for (uint32_t s = 0; s < k_max_dock_surfaces; ++s)
    {
        const int32_t root = prune(layout, layout.roots[s], 0);
        out.roots[s] = root < 0 ? k_no_node : copy_node(layout, root, out, 0);
    }

    for (uint32_t f = 0; f < layout.float_count; ++f)
        out.floats[out.float_count++] = layout.floats[f];
    for (uint32_t h = 0; h < layout.home_count; ++h)
        if (!is_open(out, layout.homes[h].panel))
            out.homes[out.home_count++] = layout.homes[h];
    for (uint32_t c = 0; c < layout.closed_count; ++c)
        if (!is_open(out, layout.closed[c]))
            out.closed[out.closed_count++] = layout.closed[c];

    layout = out;
}

void normalize(DockLayout& layout, const PanelTable& panels)
{
    normalize(layout);
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && !all_may_collapse(layout.nodes[n], panels))
            layout.nodes[n].collapsed = 0;
}

void migrate_surfaces(DockLayout& layout, const PanelTable& panels, uint32_t surface_count)
{
    for (uint32_t f = 0; f < layout.float_count && f < k_max_dock_floats; ++f)
        if (layout.floats[f].surface >= surface_count)
            layout.floats[f].surface = 0;
    for (uint32_t s = math::max(surface_count, 1u); s < k_max_dock_surfaces; ++s)
    {
        const int32_t root = layout.roots[s];
        if (root == k_no_node)
            continue;
        PanelId moved[k_max_panels];
        uint32_t moved_count = 0;
        for (uint32_t n = 0; n < layout.node_count && n < k_max_dock_nodes; ++n)
        {
            const DockNode& node = layout.nodes[n];
            if (node.kind != DockNodeKind::Tabs || root_of(layout, static_cast<int32_t>(n)) != root)
                continue;
            for (uint32_t t = 0; t < node.count && t < k_max_dock_tabs && moved_count < k_max_panels; ++t)
                moved[moved_count++] = node.tabs[t];
        }
        layout.roots[s] = k_no_node;
        normalize(layout, panels);
        for (uint32_t m = 0; m < moved_count; ++m)
        {
            const DockReason placed = layout.roots[0] == k_no_node ? insert_panel(layout, moved[m], k_no_node, DropZone::Centre) : DockReason::TargetInvalid;
            if (placed == DockReason::None)
                continue;
            for (uint32_t n = 0; n < layout.node_count; ++n)
                if (layout.nodes[n].kind == DockNodeKind::Tabs && root_of(layout, static_cast<int32_t>(n)) == layout.roots[0] && insert_panel(layout, moved[m], static_cast<int32_t>(n), DropZone::Centre) == DockReason::None)
                    break;
        }
    }
    normalize(layout, panels);
}

void validate(const DockLayout& layout)
{
    if (layout.version != k_dock_version)
        fail("version " + std::to_string(layout.version) + " is not " + std::to_string(k_dock_version));
    if (layout.node_count > k_max_dock_nodes)
        fail("node count " + std::to_string(layout.node_count) + " over capacity");
    if (layout.float_count > k_max_dock_floats)
        fail("float count " + std::to_string(layout.float_count) + " over capacity");
    if (layout.home_count > k_max_dock_homes)
        fail("home count " + std::to_string(layout.home_count) + " over capacity");
    if (layout.closed_count > k_max_dock_closed)
        fail("closed count " + std::to_string(layout.closed_count) + " over capacity");

    bool referenced[k_max_dock_nodes] = {};
    for (uint32_t s = 0; s < k_max_dock_surfaces; ++s)
    {
        if (layout.roots[s] == k_no_node)
            continue;
        validate_node(layout, layout.roots[s], referenced, 0);
    }
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (!referenced[n])
            fail("node " + std::to_string(n) + " is not reachable from any root");

    for (uint32_t f = 0; f < layout.float_count; ++f)
    {
        const DockFloat& fl = layout.floats[f];
        if (!is_valid(fl.panel))
            fail("float " + std::to_string(f) + " holds the invalid panel id");
        if (fl.surface >= k_max_dock_surfaces)
            fail("float " + std::to_string(f) + " surface out of range");
        if (!std::isfinite(fl.rect.min[0]) || !std::isfinite(fl.rect.min[1]) || !std::isfinite(fl.rect.size[0]) || !std::isfinite(fl.rect.size[1]) || !(fl.rect.size[0] > 0.0f) || !(fl.rect.size[1] > 0.0f))
            fail("float " + std::to_string(f) + " rect is not finite and positive");
    }
    for (uint32_t h = 0; h < layout.home_count; ++h)
    {
        if (!is_valid(layout.homes[h].panel))
            fail("home " + std::to_string(h) + " holds the invalid panel id");
        if (static_cast<uint8_t>(layout.homes[h].zone) > static_cast<uint8_t>(DropZone::Bottom))
            fail("home " + std::to_string(h) + " zone out of range");
    }
    for (uint32_t c = 0; c < layout.closed_count; ++c)
        if (!is_valid(layout.closed[c]))
            fail("closed entry " + std::to_string(c) + " holds the invalid panel id");

    uint32_t ids[k_max_dock_nodes * k_max_dock_tabs + k_max_dock_floats + k_max_dock_closed];
    uint32_t id_count = 0;
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs)
            for (uint32_t t = 0; t < layout.nodes[n].count; ++t)
                ids[id_count++] = layout.nodes[n].tabs[t].hash;
    for (uint32_t f = 0; f < layout.float_count; ++f)
        ids[id_count++] = layout.floats[f].panel.hash;
    for (uint32_t c = 0; c < layout.closed_count; ++c)
        ids[id_count++] = layout.closed[c].hash;
    std::sort(ids, ids + id_count);
    for (uint32_t i = 1; i < id_count; ++i)
        if (ids[i] == ids[i - 1])
            fail("panel #" + std::to_string(ids[i]) + " appears more than once");
}

void validate(const DockLayout& layout, const PanelTable& panels, ValidateFlags flags)
{
    validate(layout);
    for (uint32_t n = 0; n < layout.node_count; ++n)
        if (layout.nodes[n].kind == DockNodeKind::Tabs && layout.nodes[n].collapsed != 0 && !all_may_collapse(layout.nodes[n], panels))
            fail("tabs node " + std::to_string(n) + " is collapsed but holds a panel that may not collapse");
    if (!flags.require_viewport)
        return;
    for (uint32_t n = 0; n < layout.node_count; ++n)
    {
        if (layout.nodes[n].kind != DockNodeKind::Tabs)
            continue;
        for (uint32_t t = 0; t < layout.nodes[n].count; ++t)
        {
            const PanelDesc* desc = find_panel(panels, layout.nodes[n].tabs[t]);
            if (desc != nullptr && desc->kind == PanelKind::Viewport)
                return;
        }
    }
    for (uint32_t f = 0; f < layout.float_count; ++f)
    {
        const PanelDesc* desc = find_panel(panels, layout.floats[f].panel);
        if (desc != nullptr && desc->kind == PanelKind::Viewport)
            return;
    }
    fail("no viewport panel is open");
}

bool equal(const DockLayout& a, const DockLayout& b)
{
    if (a.version != b.version || a.node_count != b.node_count || a.float_count != b.float_count || a.home_count != b.home_count || a.closed_count != b.closed_count)
        return false;
    for (uint32_t s = 0; s < k_max_dock_surfaces; ++s)
        if (a.roots[s] != b.roots[s])
            return false;
    for (uint32_t n = 0; n < a.node_count && n < k_max_dock_nodes; ++n)
        if (!same_node(a.nodes[n], b.nodes[n]))
            return false;
    for (uint32_t f = 0; f < a.float_count && f < k_max_dock_floats; ++f)
    {
        const DockFloat& x = a.floats[f];
        const DockFloat& y = b.floats[f];
        if (x.panel != y.panel || x.surface != y.surface || !same_bits(x.rect.min[0], y.rect.min[0]) || !same_bits(x.rect.min[1], y.rect.min[1]) || !same_bits(x.rect.size[0], y.rect.size[0]) || !same_bits(x.rect.size[1], y.rect.size[1]))
            return false;
    }
    for (uint32_t h = 0; h < a.home_count && h < k_max_dock_homes; ++h)
        if (a.homes[h].panel != b.homes[h].panel || a.homes[h].sibling != b.homes[h].sibling || a.homes[h].zone != b.homes[h].zone)
            return false;
    for (uint32_t c = 0; c < a.closed_count && c < k_max_dock_closed; ++c)
        if (a.closed[c] != b.closed[c])
            return false;
    return true;
}

std::string dump(const DockLayout& layout) { return dump_impl(layout, nullptr); }
std::string dump(const DockLayout& layout, const PanelTable& panels) { return dump_impl(layout, &panels); }

std::string diff(const DockLayout& a, const DockLayout& b)
{
    const std::string text_a = dump_impl(a, nullptr);
    const std::string text_b = dump_impl(b, nullptr);
    if (text_a == text_b)
        return {};
    const std::vector<std::string_view> lines_a = split_lines(text_a);
    const std::vector<std::string_view> lines_b = split_lines(text_b);
    std::string out;
    const size_t count = math::max(lines_a.size(), lines_b.size());
    for (size_t i = 0; i < count; ++i)
    {
        const std::string_view line_a = i < lines_a.size() ? lines_a[i] : std::string_view{};
        const std::string_view line_b = i < lines_b.size() ? lines_b[i] : std::string_view{};
        if (line_a == line_b)
            continue;
        if (i < lines_a.size())
        {
            out += "-";
            out += line_a;
            out += "\n";
        }
        if (i < lines_b.size())
        {
            out += "+";
            out += line_b;
            out += "\n";
        }
    }
    return out;
}

void reset(LayoutHistory& history, const DockLayout& layout)
{
    history.snapshots[0] = layout;
    history.oldest = 0;
    history.count = 1;
    history.cursor = 0;
}

void push(LayoutHistory& history, const DockLayout& layout)
{
    if (history.count == 0)
    {
        reset(history, layout);
        return;
    }
    const uint32_t current = (history.oldest + history.cursor) % k_dock_history_size;
    if (equal(history.snapshots[current], layout))
        return;

    history.count = history.cursor + 1;
    if (history.count == k_dock_history_size)
    {
        history.oldest = (history.oldest + 1) % k_dock_history_size;
        --history.count;
    }
    history.snapshots[(history.oldest + history.count) % k_dock_history_size] = layout;
    ++history.count;
    history.cursor = history.count - 1;
}

bool can_undo(const LayoutHistory& history) { return history.count > 0 && history.cursor > 0; }
bool can_redo(const LayoutHistory& history) { return history.count > 0 && history.cursor + 1 < history.count; }

bool undo(LayoutHistory& history, DockLayout& out)
{
    if (!can_undo(history))
        return false;
    --history.cursor;
    out = history.snapshots[(history.oldest + history.cursor) % k_dock_history_size];
    return true;
}

bool redo(LayoutHistory& history, DockLayout& out)
{
    if (!can_redo(history))
        return false;
    ++history.cursor;
    out = history.snapshots[(history.oldest + history.cursor) % k_dock_history_size];
    return true;
}

DockLayout build_default_layout(const PanelTable& panels)
{
    DockLayout layout;
    uint32_t order[k_max_panels] = {};
    const uint32_t count = panels.count < k_max_panels ? panels.count : k_max_panels;
    for (uint32_t i = 0; i < count; ++i)
        order[i] = i;
    std::stable_sort(order, order + count, [&panels](uint32_t a, uint32_t b) { return panels.descs[a].order < panels.descs[b].order; });
    for (uint32_t i = 0; i < count; ++i)
    {
        const PanelDesc& desc = panels.descs[order[i]];
        if (desc.initial_open)
            static_cast<void>(open_panel(layout, panels, desc.id));
    }
    normalize(layout);
    return layout;
}

}
