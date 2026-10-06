#include "oxpch.h"
#include "Oryx/Renderer/GraphicsSettings.h"

namespace oryx
{

namespace
{

int32_t non_negative(const SettingsNode& node, std::string_view key, int32_t fallback)
{
    const int64_t value = node.integer(key, fallback);
    if (value < 0 || value > std::numeric_limits<int32_t>::max())
    {
        throw SettingsError("graphics." + std::string(key) + " must be between 0 and " + std::to_string(std::numeric_limits<int32_t>::max()) + ", got " + std::to_string(value));
    }
    return static_cast<int32_t>(value);
}

} // namespace

KeyCode key_from_name(std::string_view name)
{
    if (name.size() == 1 && std::isalpha(static_cast<unsigned char>(name[0])))
    {
        return static_cast<KeyCode>(static_cast<int32_t>(KeyCode::A) + (std::toupper(static_cast<unsigned char>(name[0])) - 'A'));
    }
    if ((name.size() == 2 || name.size() == 3) && (name[0] == 'F' || name[0] == 'f'))
    {
        int32_t number = 0;
        for (size_t i = 1; i < name.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(name[i])))
            {
                return KeyCode::Unknown;
            }
            number = number * 10 + (name[i] - '0');
        }
        if (number >= 1 && number <= 12)
        {
            return static_cast<KeyCode>(static_cast<int32_t>(KeyCode::F1) + number - 1);
        }
    }
    return KeyCode::Unknown;
}

void read_settings(GraphicsSettings& settings, const SettingsNode& node)
{
    settings.vsync = node.boolean("vsync", settings.vsync);
    settings.max_fps = non_negative(node, "max_fps", settings.max_fps);
    settings.idle_sleep_ms = non_negative(node, "idle_sleep_ms", settings.idle_sleep_ms);
    settings.reload_key = node.string("reload_key", settings.reload_key);
    if (!settings.reload_key.empty() && key_from_name(settings.reload_key) == KeyCode::Unknown)
    {
        throw SettingsError("graphics.reload_key must be F1-F12, a letter or empty, got '" + settings.reload_key + "'");
    }
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::GraphicsSettings, "graphics")
