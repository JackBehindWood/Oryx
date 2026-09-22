#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `scripting:` section of the settings file.
struct ScriptSettings
{
    // Whether scripting is active at all; false skips script discovery/loading entirely.
    bool enabled = true;

    // Directories whose scripts are loaded at startup and on reload, and that scripts import each other from.
    std::vector<std::filesystem::path> roots;
};

void read_settings(ScriptSettings& settings, const SettingsNode& node);

} // namespace oryx
