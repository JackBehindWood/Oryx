#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `resources:` section: one folder per application holding source assets and, under compiled/, their derived counterparts.
struct ResourceSettings
{
    // A relative root is relative to the settings file.
    std::filesystem::path root = "resources";
    bool compiled_enabled = true;
};

void read_settings(ResourceSettings& settings, const SettingsNode& node);

[[nodiscard]] std::filesystem::path compiled_directory(const ResourceSettings& settings);

} // namespace oryx
