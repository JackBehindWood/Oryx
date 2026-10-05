#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `assets:` section of the settings file.
struct AssetSettings
{
    // Directories searched in order when resolving a relative asset path; a relative root is relative to the settings file.
    std::vector<std::filesystem::path> roots;

    std::filesystem::path cache_dir = ".cache/assets";
    bool cache_enabled = true;

    // 0 runs requests on the main thread inside Assets::update(); larger values are not implemented yet.
    uint32_t worker_threads = 0;
};

void read_settings(AssetSettings& settings, const SettingsNode& node);

} // namespace oryx
