#include "oxpch.h"
#include "Oryx/Assets/AssetSettings.h"

namespace oryx
{

void read_settings(AssetSettings& settings, const SettingsNode& node)
{
    settings.roots = node.paths("roots");
    settings.compiled_enabled = node.boolean("compiled_enabled", settings.compiled_enabled);
    int64_t threads = node.integer("worker_threads", 0);
    if (threads < 0)
    {
        throw SettingsError("assets.worker_threads must not be negative");
    }
    settings.worker_threads = static_cast<uint32_t>(threads);
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::AssetSettings, "assets")
