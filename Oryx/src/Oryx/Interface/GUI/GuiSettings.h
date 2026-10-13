#pragma once

#include "Oryx/Core/Settings.h"
#include "Oryx/Interface/GUI/Dock/DockSolve.h"

namespace oryx
{

// The `gui:` section: whether panels dock at all, where the dock layout is saved (an empty layout_file keeps it in memory only) and the dock look-and-feel (dock_* keys, read live every frame). dock_preview_opacity is a percentage.
struct GuiSettings
{
    bool docking = true;
    std::filesystem::path layout_file = "gui-layout.yaml";
    gui::DockStyle dock;
};

template<typename Visitor>
void reflect(Visitor& v, GuiSettings& s)
{
    gui::DockStyle& dock = s.dock;
    v.field("docking", s.docking, Doc{ "Panels dock; off keeps the viewport panel alone" });
    v.field("layout_file", s.layout_file, Doc{ "Where the dock layout is saved (empty keeps it in memory)" });
    v.field("dock_toolbars", dock.toolbars, Doc{ "Panel toolbars" });
    v.field("dock_toolbar_placement", dock.toolbar_placement, choices<gui::ToolbarPlacement>({ { "auto", gui::ToolbarPlacement::Auto }, { "above_tabs", gui::ToolbarPlacement::AboveTabs }, { "below_tabs", gui::ToolbarPlacement::BelowTabs } }), Doc{ "Toolbar position relative to the tab strip" });
    v.field("dock_tab_position", dock.tab_position, choices<gui::TabPosition>({ { "top", gui::TabPosition::Top }, { "bottom", gui::TabPosition::Bottom } }), Doc{ "Tab strip edge" });
    v.field("dock_close_buttons", dock.close_buttons, choices<gui::CloseButtons>({ { "on_hover", gui::CloseButtons::OnHover }, { "always", gui::CloseButtons::Always }, { "never", gui::CloseButtons::Never } }), Doc{ "When tabs show a close button" });
    v.field("dock_tear_off", dock.tear_off, choices<gui::TearOff>({ { "live", gui::TearOff::Live }, { "on_release", gui::TearOff::OnRelease }, { "never", gui::TearOff::Never } }), Doc{ "When a dragged tab becomes a floating panel" });
    v.field("dock_tab_cycle", dock.tab_cycle, choices<gui::TabCycle>({ { "layout", gui::TabCycle::Layout }, { "recent", gui::TabCycle::Recent } }), Doc{ "Ctrl+Tab order" });
    v.field("dock_compact", dock.compact, Doc{ "Smaller strips and toolbars" });
    v.field("dock_preview_opacity", dock.preview_opacity, Percent{}, Doc{ "Opacity of the drop preview, in percent" });
    v.field("dock_guides", dock.guides, Doc{ "Drop guides while dragging a panel" });
    v.field("dock_drop_flash", dock.drop_flash, Doc{ "Accent flash on the panel that just docked" });
}

} // namespace oryx
