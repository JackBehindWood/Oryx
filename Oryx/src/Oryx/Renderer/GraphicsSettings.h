#pragma once

#include "Oryx/Core/KeyCode.h"
#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `graphics:` section. max_fps 0 means uncapped; idle_sleep_ms applies only to a frame that was not presented; an empty reload_key disables shader reload.
struct GraphicsSettings
{
    bool vsync = true;
    int32_t max_fps = 0;
    int32_t idle_sleep_ms = 50;
    std::string reload_key = "F5";
};

void read_settings(GraphicsSettings& settings, const SettingsNode& node);

// F1-F12 or a single letter, any case; KeyCode::Unknown for anything else (and for an empty name).
[[nodiscard]] KeyCode key_from_name(std::string_view name);

} // namespace oryx
