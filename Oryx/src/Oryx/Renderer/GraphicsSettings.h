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

// F1-F12 or a single letter, any case; KeyCode::Unknown for anything else (and for an empty name).
[[nodiscard]] KeyCode key_from_name(std::string_view name);

template<typename Visitor>
void reflect(Visitor& v, GraphicsSettings& s)
{
    v.field("vsync", s.vsync, Doc{ "Synchronise presentation with the display" });
    v.field("max_fps", s.max_fps, Range{ 0, INT32_MAX }, Doc{ "Frame rate cap; 0 is uncapped" });
    v.field("idle_sleep_ms", s.idle_sleep_ms, Range{ 0, INT32_MAX }, Doc{ "Sleep after a frame that was not presented" });
    v.field("reload_key", s.reload_key,
            Valid{ [](std::string_view name) { return name.empty() || key_from_name(name) != KeyCode::Unknown; }, "F1-F12, a letter or empty" },
            Doc{ "Shader reload key; empty disables reload" });
}

} // namespace oryx
