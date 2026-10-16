#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `dashboard:` section. history is the feed's record capacity; max_tree_nodes is the search nodes kept per record (0 keeps none); annotations are the board overlays switched on at start (heat, chosen, value). replay_window is the decisions a replay keeps resident (a longer log streams through it); replay_max_records caps how many decisions a replay indexes (8 bytes each). Layout file and dock look-and-feel belong to the GUI (`gui:`).
struct DashboardSettings
{
    int32_t history = 256;
    int32_t max_tree_nodes = 256;
    int32_t replay_window = 4096;
    int32_t replay_max_records = 10'000'000;
    std::vector<std::string> annotations = { "heat", "chosen" };
};

template<typename Visitor>
void reflect(Visitor& v, DashboardSettings& s)
{
    v.removed("enabled", "the dashboard's views are added in code (Dashboard::add_view) and View > Window opens and closes it");
    v.removed("views", "the dashboard's views are added in code (Dashboard::add_view) and View > Window opens and closes it");
    v.field("history", s.history, Range{ 1, INT32_MAX }, Doc{ "Decisions the feed keeps" });
    v.field("max_tree_nodes", s.max_tree_nodes, Range{ 0, 65536 }, Doc{ "Search tree nodes kept per decision (0 keeps none)" });
    v.field("replay_window", s.replay_window, Range{ 1, 1 << 20 }, Doc{ "Decisions a replay keeps in memory" });
    v.field("replay_max_records", s.replay_max_records, Range{ 1, INT32_MAX }, Doc{ "Decisions a replay indexes; a longer log is cut off" });
    v.field("annotations", s.annotations, one_of({ "heat", "chosen", "value" }), Doc{ "Board overlays on at start: heat, chosen, value" });
}

} // namespace oryx
