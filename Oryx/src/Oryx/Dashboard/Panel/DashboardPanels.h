#pragma once

#include "Oryx/Dashboard/DashboardSettings.h"
#include "Oryx/Dashboard/Panel/DashboardPanel.h"
#include "Oryx/Interface/GUI/Dock/GuiPanelHost.h"

namespace oryx
{

inline constexpr const char* k_viewport_panel = "viewport/0";
inline constexpr const char* k_dashboard_group = "dashboard";

// Registers the board's viewport panel and one panel per view on the active GuiContext, with placement hints only: the first view beside the board at 360 pt, later views tabbed with it, all in group `dashboard`
// and open in the default layout when `enabled`. Safe to repeat: a name already registered is left alone, so a view added later only adds its panel.
void register_dashboard_panels(const DashboardPanelState& state, bool enabled);

// Draws view `index` as the dock panel of that name, with its match and decision counters in the toolbar strip. Call inside a panel host; order across views does not matter.
void draw_dashboard_panel(DashboardPanelState& state, size_t index, DashboardModel& model);

} // namespace oryx
