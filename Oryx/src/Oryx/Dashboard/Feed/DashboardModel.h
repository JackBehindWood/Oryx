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
    // Set by the host each frame: this view's panel has focus and no text field wants the keys.
    bool keyboard_focus = false;
    // Plain action ids so panels and the board can point at each other without naming a space: the Decision row under the pointer (cleared by the host each frame), the row a click pinned, and the action whose destination the board pointer is over.
    ActionId hovered_action = INVALID_ACTION;
    ActionId pinned_action = INVALID_ACTION;
    ActionId board_hovered_action = INVALID_ACTION;
};

inline void select_record(DashboardModel& model, uint64_t sequence)
{
    model.selected_sequence = sequence;
    model.follow_latest = false;
}

// The action the board should single out: the hovered row, else the pinned one.
[[nodiscard]] inline ActionId linked_action(const DashboardModel& model)
{
    return is_game_action(model.hovered_action) ? model.hovered_action : model.pinned_action;
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

// A selection that left the ring (wrapped out or cleared by a game switch) returns to following, so no view is stranded on "No decision".
inline void sanitise_selection(DashboardModel& model)
{
    if (!model.follow_latest && !selected_view(model).valid)
    {
        model.follow_latest = true;
    }
}

} // namespace oryx
