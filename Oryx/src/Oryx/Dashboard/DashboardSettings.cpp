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
    settings.history = at_least_one(node, "history", settings.history);
    const int64_t tree_nodes = node.integer("max_tree_nodes", settings.max_tree_nodes);
    if (tree_nodes < 0 || tree_nodes > 65536)
    {
        throw SettingsError("dashboard.max_tree_nodes must be between 0 and 65536, got " + std::to_string(tree_nodes));
    }
    settings.max_tree_nodes = static_cast<int32_t>(tree_nodes);
    if (node.has("annotations"))
    {
        settings.annotations = node.strings("annotations");
        for (const std::string& name : settings.annotations)
        {
            if (name != "heat" && name != "chosen" && name != "value")
            {
                throw SettingsError("dashboard.annotations: unknown overlay '" + name + "' (heat, chosen or value)");
            }
        }
    }
    if (node.has("views"))
    {
        settings.views = node.strings("views");
    }
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::DashboardSettings, "dashboard")
