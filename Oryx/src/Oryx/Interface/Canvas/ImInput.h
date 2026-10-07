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

// One frame of input for one surface. Built by the owning layer from the window's input; `text` is valid for the frame only.
struct ImInput
{
    uint32_t surface = 0;
    // Logical points; the viewport layout is solved in. Zero leaves layout with no room.
    Vec2f surface_size{ 0.0f, 0.0f };
    // Device pixels per logical point.
    float scale = 1.0f;
    ImPointer pointer;
    Vec2f wheel{ 0.0f, 0.0f };
    float delta_time = 0.0f;
    std::string_view text;
};

[[nodiscard]] inline const ImButton& button_of(const ImInput& input, MouseCode code)
{
    return input.pointer.buttons[static_cast<uint32_t>(code)];
}

} // namespace oryx
