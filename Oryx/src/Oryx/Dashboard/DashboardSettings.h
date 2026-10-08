#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `dashboard:` section. panel_width is logical points; history is the feed's record capacity; views are registered view ids in display order.
struct DashboardSettings
{
    bool enabled = false;
    int32_t panel_width = 360;
    int32_t history = 256;
    std::vector<std::string> views = { "probabilities", "values" };
    std::filesystem::path layout_file = "dashboard-layout.yaml";
};

void read_settings(DashboardSettings& settings, const SettingsNode& node);

} // namespace oryx
