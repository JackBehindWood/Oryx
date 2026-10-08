#pragma once

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx
{

// The views a panel shows, in display order.
struct DashboardPanelState
{
    std::vector<UniquePtr<IDashboardView>> views;
};

// Creates the registered views named by `view_ids`; an unknown id is logged and skipped.
[[nodiscard]] DashboardPanelState make_panel_state(const std::vector<std::string>& view_ids);

// Draws the views on the active GuiContext, each under a collapsible header; the caller supplies the container (a side panel, a dock tab).
void draw_dashboard(DashboardPanelState& state, DashboardModel& model);

} // namespace oryx
