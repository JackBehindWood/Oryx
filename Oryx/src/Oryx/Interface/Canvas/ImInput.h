#pragma once

#include "Oryx/Core/MouseCode.h"
#include "Oryx/Math/Vector.h"

namespace oryx
{

inline constexpr uint32_t k_im_button_count = 3;

struct ImButton
{
    bool down = false;
    bool pressed = false;
    bool released = false;
};

// Pointer position in logical points of the surface, origin top-left. `valid` is false while the pointer is outside the surface.
struct ImPointer
{
    Vec2f position{ 0.0f, 0.0f };
    bool valid = false;
    ImButton buttons[k_im_button_count];
};

enum class ImKey : uint8_t
{
    Escape,
    Enter,
    Tab,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PageUp,
    PageDown,
    Space,
    A,
    C,
    V,
    X,
    Count
};

[[nodiscard]] constexpr uint32_t im_key_bit(ImKey key) { return 1u << static_cast<uint32_t>(key); }

// Bit per ImKey: `down` while held, `pressed` only on the frame it went down. Modifiers merge the left and right keys.
struct ImKeys
{
    uint32_t down = 0;
    uint32_t pressed = 0;
    // Keys the OS auto-repeated this frame (held past the delay); a text field treats pressed or repeated as one stroke.
    uint32_t repeated = 0;
    // The platform's command modifier: Cmd on macOS, Ctrl elsewhere.
    bool shortcut = false;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    bool super = false;
};

[[nodiscard]] constexpr bool key_down(const ImKeys& keys, ImKey key) { return (keys.down & im_key_bit(key)) != 0; }
[[nodiscard]] constexpr bool key_pressed(const ImKeys& keys, ImKey key) { return (keys.pressed & im_key_bit(key)) != 0; }
[[nodiscard]] constexpr bool key_stroke(const ImKeys& keys, ImKey key) { return ((keys.pressed | keys.repeated) & im_key_bit(key)) != 0; }

// One frame of input for one surface. Built by the owning layer from the window's input; `text` is valid for the frame only.
struct ImInput
{
    uint32_t surface = 0;
    // Logical points; the viewport layout is solved in. Zero leaves layout with no room.
    Vec2f surface_size{ 0.0f, 0.0f };
    // Device pixels per logical point.
    float scale = 1.0f;
    ImPointer pointer;
    // In lines: one mouse notch is 1.0, trackpads give fractions; the OS's scroll direction is passed through.
    Vec2f wheel{ 0.0f, 0.0f };
    ImKeys keys;
    float delta_time = 0.0f;
    std::string_view text;
    // The clipboard text on the frame the paste shortcut went down, empty otherwise; valid for the frame only.
    std::string_view paste;
};

[[nodiscard]] inline const ImButton& button_of(const ImInput& input, MouseCode code)
{
    return input.pointer.buttons[static_cast<uint32_t>(code)];
}

} // namespace oryx
