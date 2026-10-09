#include "oxpch.h"
#include "Oryx/Dashboard/Panel/DashboardPanels.h"

namespace oryx
{

namespace
{

constexpr float k_view_min_width = 160.0f;
constexpr float k_view_min_height = 96.0f;
constexpr float k_board_min_size = 200.0f;
constexpr float k_views_width = 360.0f;

void draw_counters(const DashboardModel& model)
{
    if (model.feed == nullptr)
    {
        gui::label("No active match");
        return;
    }
    const DashboardFeed& feed = *model.feed;
    if (feed.size() == 0)
    {
        gui::label("Waiting for the first decision");
        return;
    }
    FrameArena& arena = gui::context().arena();
    const DashboardRecord& latest = *feed.view(feed.size() - 1).record;
    gui::key_value("Match", arena.format("%u", latest.match_index + 1));
    gui::key_value("Decisions", arena.format("%llu", static_cast<unsigned long long>(feed.total_decisions())));
}

} // namespace

void register_dashboard_panels(const DashboardPanelState& state, bool enabled)
{
    gui::PanelOptions board;
    board.title = "Board";
    board.kind = gui::PanelKind::Viewport;
    board.min_w = k_board_min_size;
    board.min_h = k_board_min_size;
    static_cast<void>(gui::register_panel(k_viewport_panel, board));
    for (size_t index = 0; index < state.views.size(); ++index)
    {
        gui::PanelOptions options;
        options.title = state.views[index]->title();
        options.min_w = k_view_min_width;
        options.min_h = k_view_min_height;
        options.toolbar = true;
        options.group = k_dashboard_group;
        options.initial_open = enabled;
        options.dock_near = k_viewport_panel;
        options.dock_side = gui::DropZone::Right;
        options.dock_size = k_views_width;
        options.order = static_cast<int32_t>(index) + 1;
        if (index > 0)
        {
            options.dock_tabbed_with = state.panels[0];
        }
        static_cast<void>(gui::register_panel(state.panels[index], options));
    }
}

void draw_dashboard_panel(DashboardPanelState& state, size_t index, DashboardModel& model)
{
    if (gui::begin_panel(state.panels[index]))
    {
        if (gui::PanelToolbarScope toolbar; toolbar.visible())
        {
            draw_counters(model);
        }
        if (model.feed != nullptr)
        {
            state.views[index]->draw(model);
        }
    }
    gui::end_panel();
}

} // namespace oryx
