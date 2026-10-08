#pragma once

#include "Oryx/Dashboard/Feed/DashboardFeed.h"

namespace oryx
{

// What the dashboard is looking at; owned by the panel, not the feed, so several surfaces can share one selection.
struct DashboardModel
{
    const DashboardFeed* feed = nullptr;
    uint64_t selected_sequence = 0;
    bool follow_latest = true;
};

inline void select_record(DashboardModel& model, uint64_t sequence)
{
    model.selected_sequence = sequence;
    model.follow_latest = false;
}

inline void follow_latest_record(DashboardModel& model)
{
    model.follow_latest = true;
}

// Invalid when nothing is held or the selected record has left the ring.
[[nodiscard]] inline RecordView selected_view(const DashboardModel& model)
{
    if (model.feed == nullptr)
    {
        return RecordView{};
    }
    if (model.follow_latest)
    {
        return model.feed->size() == 0 ? RecordView{} : model.feed->view(model.feed->size() - 1);
    }
    return model.feed->find_sequence(model.selected_sequence);
}

} // namespace oryx
