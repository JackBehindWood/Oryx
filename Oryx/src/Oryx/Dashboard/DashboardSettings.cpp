#include "oxpch.h"
#include "Oryx/Dashboard/DashboardSettings.h"

namespace oryx
{

namespace
{

int32_t at_least_one(const SettingsNode& node, std::string_view key, int32_t fallback)
{
    const int64_t value = node.integer(key, fallback);
    if (value < 1 || value > std::numeric_limits<int32_t>::max())
    {
        throw SettingsError("dashboard." + std::string(key) + " must be between 1 and " + std::to_string(std::numeric_limits<int32_t>::max()) + ", got " + std::to_string(value));
    }
    return static_cast<int32_t>(value);
}

} // namespace

void read_settings(DashboardSettings& settings, const SettingsNode& node)
{
    settings.enabled = node.boolean("enabled", settings.enabled);
    settings.panel_width = at_least_one(node, "panel_width", settings.panel_width);
    settings.history = at_least_one(node, "history", settings.history);
    if (node.has("views"))
    {
        settings.views = node.strings("views");
    }
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::DashboardSettings, "dashboard")
