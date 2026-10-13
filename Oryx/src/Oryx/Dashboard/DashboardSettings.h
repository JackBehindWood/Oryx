#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `dashboard:` section. history is the feed's record capacity; max_tree_nodes is the search nodes kept per record (0 keeps none); annotations are the board overlays switched on at start (heat, chosen, value). Layout file and dock look-and-feel belong to the GUI (`gui:`).
struct DashboardSettings
{
    int32_t history = 256;
    int32_t max_tree_nodes = 256;
    std::vector<std::string> annotations = { "heat", "chosen" };
};

void read_settings(DashboardSettings& settings, const SettingsNode& node);

} // namespace oryx
