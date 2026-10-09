#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `dashboard:` section. history is the feed's record capacity; views are registered view ids that become panels, in registration order. Layout file and dock look-and-feel belong to the GUI (`gui:`).
struct DashboardSettings
{
    bool enabled = false;
    int32_t history = 256;
    std::vector<std::string> views = { "probabilities", "values" };
};

void read_settings(DashboardSettings& settings, const SettingsNode& node);

} // namespace oryx
