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
    FrameArena& arena = gui::context().arena();
    const DashboardFeed& feed = *model.feed;
    if (feed.size() == 0)
    {
        gui::label("Waiting for the first decision");
    }
    else
    {
        const DashboardRecord& latest = *feed.view(feed.size() - 1).record;
        gui::key_value("Match", arena.format("%u", latest.match_index + 1));
        gui::key_value("Decisions", arena.format("%llu", static_cast<unsigned long long>(feed.total_decisions())));
    }
    for (UniquePtr<IDashboardView>& view : state.views)
    {
        gui::separator();
        if (gui::collapsing_header(view->title(), true))
        {
            view->draw(model);
        }
    }
}

} // namespace oryx
