#include "oxpch.h"
#include "Oryx/Dashboard/Panel/DashboardDefaultLayout.h"

#include "Oryx/Interface/GUI/Dock/DockOps.h"

namespace oryx
{

namespace
{

constexpr float k_board_ratio = 0.7f;
constexpr float k_views_width = 360.0f;

int32_t tabs_holding(const gui::DockLayout& layout, gui::PanelId panel)
{
    for (uint32_t n = 0; n < layout.node_count; ++n)
    {
        const gui::DockNode& node = layout.nodes[n];
        if (node.kind != gui::DockNodeKind::Tabs)
            continue;
        for (uint32_t t = 0; t < node.count; ++t)
            if (node.tabs[t] == panel)
                return static_cast<int32_t>(n);
    }
    return gui::k_no_node;
}

} // namespace

gui::DockLayout default_dashboard_layout(const gui::PanelTable& panels)
{
    gui::DockLayout layout;
    const gui::PanelId viewport = gui::make_panel_id(k_viewport_panel);
    const bool has_viewport = gui::find_panel(panels, viewport) != nullptr;
    if (has_viewport)
        static_cast<void>(gui::dock_panel(layout, panels, viewport, gui::k_dock_root, gui::DropZone::Centre));

    gui::PanelId first_view;
    for (const char* name : { k_probabilities_panel, k_values_panel })
    {
        const gui::PanelId id = gui::make_panel_id(name);
        if (gui::find_panel(panels, id) == nullptr)
            continue;
        if (!is_valid(first_view))
        {
            first_view = id;
            static_cast<void>(gui::dock_panel(layout, panels, id, gui::k_dock_root, has_viewport ? gui::DropZone::Right : gui::DropZone::Centre));
        }
        else
        {
            static_cast<void>(gui::dock_panel(layout, panels, id, tabs_holding(layout, first_view), gui::DropZone::Centre));
        }
    }

    if (has_viewport && is_valid(first_view))
        static_cast<void>(gui::set_split(layout, panels, layout.roots[0], gui::DockSizeMode::FixedSecond, k_board_ratio, k_views_width));
    gui::normalize(layout);
    return layout;
}

} // namespace oryx
