#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/DockBuilder.h"

#include "Oryx/Interface/GUI/Dock/GuiPanelHost.h"

namespace oryx::gui
{

DockBuilder::DockBuilder()
{
    m_entries.emplace_back();
}

DockBuilder::Entry& DockBuilder::leaf(DockNodeRef node, const char* verb)
{
    if (node.index < 0 || static_cast<size_t>(node.index) >= m_entries.size())
        throw Error(std::string("DockBuilder::") + verb + " got a node from another builder", "use the refs returned by root() and split()");
    Entry& entry = m_entries[node.index];
    if (entry.is_split)
        throw Error(std::string("DockBuilder::") + verb + " needs a tab stack, not a split", "use the node split() returned, not a parent");
    return entry;
}

DockNodeRef DockBuilder::split(DockNodeRef node, DropZone zone, float points)
{
    if (zone == DropZone::Centre)
        throw Error("DockBuilder::split needs an edge zone", "use Left, Right, Top or Bottom");
    static_cast<void>(leaf(node, "split"));
    const int32_t target = node.index;
    const int32_t fresh = static_cast<int32_t>(m_entries.size());
    const int32_t parent_split = fresh + 1;
    m_entries.emplace_back();
    m_entries.emplace_back();
    Entry& created = m_entries[fresh];
    Entry& wrapper = m_entries[parent_split];
    Entry& original = m_entries[target];
    wrapper.is_split = true;
    wrapper.parent = original.parent;
    wrapper.zone = zone;
    wrapper.points = points;
    wrapper.first = (zone == DropZone::Left || zone == DropZone::Top) ? fresh : target;
    wrapper.second = wrapper.first == fresh ? target : fresh;
    created.parent = parent_split;
    if (original.parent >= 0)
    {
        Entry& up = m_entries[original.parent];
        (up.first == target ? up.first : up.second) = parent_split;
    }
    else
    {
        m_root = parent_split;
    }
    original.parent = parent_split;
    return DockNodeRef{ fresh };
}

DockBuilder& DockBuilder::dock(DockNodeRef node, std::string_view panel)
{
    leaf(node, "dock").panels.emplace_back(panel);
    return *this;
}

DockBuilder& DockBuilder::collapse(DockNodeRef node, bool collapsed)
{
    leaf(node, "collapse").collapsed = collapsed;
    return *this;
}

int32_t DockBuilder::emit(const PanelTable& panels, DockLayout& out, int32_t index) const
{
    const Entry& entry = m_entries[index];
    if (out.node_count >= k_max_dock_nodes)
        throw Error("DockBuilder layout needs more than " + std::to_string(k_max_dock_nodes) + " nodes", "dock fewer panels or split less");
    const int32_t slot = static_cast<int32_t>(out.node_count++);
    if (!entry.is_split)
    {
        if (entry.panels.size() > k_max_dock_tabs)
            throw Error("DockBuilder tab stack holds " + std::to_string(entry.panels.size()) + " panels, the limit is " + std::to_string(k_max_dock_tabs), "split the stack");
        DockNode node;
        node.kind = DockNodeKind::Tabs;
        node.count = static_cast<uint32_t>(entry.panels.size());
        node.selected = node.count > 0 ? node.count - 1 : 0;
        node.collapsed = entry.collapsed ? 1 : 0;
        for (uint32_t t = 0; t < node.count; ++t)
        {
            const PanelId id = make_panel_id(entry.panels[t]);
            if (find_panel(panels, id) == nullptr)
                throw Error("DockBuilder names unknown panel '" + entry.panels[t] + "'", "register it with register_panel before build()");
            node.tabs[t] = id;
        }
        out.nodes[slot] = node;
        return slot;
    }
    const bool fresh_first = entry.zone == DropZone::Left || entry.zone == DropZone::Top;
    const int32_t fresh_index = fresh_first ? entry.first : entry.second;
    const int32_t kept_index = fresh_first ? entry.second : entry.first;
    const int32_t fresh_slot = emit(panels, out, fresh_index);
    const int32_t kept_slot = emit(panels, out, kept_index);
    DockNode node;
    init_split(node, entry.zone, fresh_slot, kept_slot, entry.points);
    out.nodes[slot] = node;
    return slot;
}

DockLayout DockBuilder::build(const PanelTable& panels) const
{
    DockLayout layout;
    layout.roots[0] = emit(panels, layout, m_root);
    std::vector<PanelId> seen;
    for (uint32_t n = 0; n < layout.node_count; ++n)
    {
        const DockNode& node = layout.nodes[n];
        for (uint32_t t = 0; node.kind == DockNodeKind::Tabs && t < node.count; ++t)
        {
            if (std::find(seen.begin(), seen.end(), node.tabs[t]) != seen.end())
                throw Error(std::string("DockBuilder docks panel '") + find_panel(panels, node.tabs[t])->name + "' twice", "a panel lives in one stack");
            seen.push_back(node.tabs[t]);
        }
    }
    normalize(layout, panels);
    bool viewport = false;
    for (uint32_t i = 0; i < panels.count; ++i)
        viewport = viewport || panels.descs[i].kind == PanelKind::Viewport;
    validate(layout, panels, ValidateFlags{ viewport });
    return layout;
}

DockLayout DockBuilder::build() const
{
    return build(panels());
}

}
