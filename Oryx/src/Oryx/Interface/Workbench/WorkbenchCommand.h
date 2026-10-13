#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Interface/Canvas/ImInput.h"

namespace oryx
{

inline constexpr uint32_t k_max_workbench_commands = 16;
inline constexpr uint32_t k_max_workbench_panels = 16;
inline constexpr uint32_t k_invalid_workbench_index = UINT32_MAX;

// A command of one workbench, by position in its table; made by Workbench::add_command.
struct CommandHandle
{
    uint32_t index = k_invalid_workbench_index;

    [[nodiscard]] bool valid() const { return index != k_invalid_workbench_index; }
    [[nodiscard]] bool operator==(const CommandHandle& other) const { return index == other.index; }
};

struct PanelHandle
{
    uint32_t index = k_invalid_workbench_index;

    [[nodiscard]] bool valid() const { return index != k_invalid_workbench_index; }
    [[nodiscard]] bool operator==(const PanelHandle& other) const { return index == other.index; }
};

// A key with the platform's command modifier (Cmd on macOS, Ctrl elsewhere), Shift and Alt as asked; `key == ImKey::Count` is no shortcut.
struct Shortcut
{
    ImKey key = ImKey::Count;
    bool shortcut = false;
    bool shift = false;
    bool alt = false;
};

[[nodiscard]] constexpr bool has_shortcut(const Shortcut& shortcut) { return shortcut.key != ImKey::Count; }
[[nodiscard]] bool shortcut_pressed(const Shortcut& shortcut, const ImKeys& keys);
// "Cmd+Shift+K" style text for a menu hint; empty without a shortcut.
[[nodiscard]] std::string shortcut_text(const Shortcut& shortcut);

// What add_command takes. `id` is the command's name in the console (`<workbench>.<id>`); a `global` shortcut works whichever panel has focus.
struct CommandSpec
{
    std::string_view id;
    std::string_view label;
    Shortcut shortcut;
    bool global = false;
};

struct CommandInfo
{
    std::string id;
    std::string label;
    Shortcut shortcut;
    bool global = false;
    bool enabled = true;
};

// The words after the command's name when it came from the console; empty for a menu or shortcut.
struct CommandArgs
{
    const std::string_view* values = nullptr;
    uint32_t count = 0;
};

} // namespace oryx
