#include "oxpch.h"
#include "Oryx/Dashboard/Panel/DashboardPanel.h"

namespace oryx
{

DashboardPanelState make_panel_state(const std::vector<std::string>& view_ids)
{
    DashboardPanelState state;
    for (const std::string& id : view_ids)
    {
        UniquePtr<IDashboardView> view = DashboardViewRegistry::create(id);
        if (view == nullptr)
        {
            OX_CORE_WARN("Dashboard view '{}' is not registered", id);
            continue;
        }
        state.views.push_back(std::move(view));
        state.panels.push_back("view/" + id);
    }
    return state;
}

void draw_dashboard(DashboardPanelState& state, DashboardModel& model)
{
    if (state.views.empty())
    {
        gui::label("No dashboard views");
        return;
    }
    if (model.feed == nullptr)
    {
        gui::label("No active match");
        return;
    }
    for (UniquePtr<IDashboardView>& view : state.views)
    {
        gui::separator();
        if (gui::collapsing_header(view->title(), true))
        {
            gui::push_id(view->title());
            view->draw(model);
            gui::pop_id();
        }
    }
}

} // namespace oryx
