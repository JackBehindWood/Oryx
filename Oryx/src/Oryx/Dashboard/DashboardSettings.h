#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `dashboard:` section. history is the feed's record capacity; views are registered view ids that become panels, in registration order; annotations are the board overlays switched on at start (heat, chosen, value). Layout file and dock look-and-feel belong to the GUI (`gui:`).
struct DashboardSettings
{
    bool enabled = false;
    int32_t history = 256;
    std::vector<std::string> annotations = { "heat", "chosen" };
    std::vector<std::string> views = { "trace", "decision", "stats" };
};

void read_settings(DashboardSettings& settings, const SettingsNode& node);

} // namespace oryx
