#include "oxpch.h"
#include "Oryx/Core/ResourceSettings.h"

namespace oryx
{

void read_settings(ResourceSettings& settings, const SettingsNode& node)
{
    settings.root = node.path("root", settings.root);
    settings.compiled_enabled = node.boolean("compiled_enabled", settings.compiled_enabled);
}

std::filesystem::path compiled_directory(const ResourceSettings& settings)
{
    return settings.root / "compiled";
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::ResourceSettings, "resources")
