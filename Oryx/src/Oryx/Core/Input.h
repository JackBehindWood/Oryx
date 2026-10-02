#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/KeyCode.h"
#include "Oryx/Core/MouseCode.h"
#include "Oryx/Math/Vector2.h"

namespace oryx
{

// Cursor coordinates are logical (window points), origin top-left.
class IInput
{
public:
    virtual ~IInput() = default;

    [[nodiscard]] virtual bool key_down(KeyCode key) const = 0;
    [[nodiscard]] virtual bool key_pressed(KeyCode key) const = 0;
    [[nodiscard]] virtual bool key_released(KeyCode key) const = 0;
    [[nodiscard]] virtual bool mouse_down(MouseCode button) const = 0;
    [[nodiscard]] virtual bool mouse_pressed(MouseCode button) const = 0;
    [[nodiscard]] virtual bool mouse_released(MouseCode button) const = 0;
    virtual void cursor_position(Vec2f& out) const = 0;
    virtual void scroll_delta(Vec2f& out) const = 0;
};

// Static shortcut onto the primary window's input; everything reads as "nothing pressed" while there is no window.
class Input
{
public:
    [[nodiscard]] static bool key_down(KeyCode key);
    [[nodiscard]] static bool key_pressed(KeyCode key);
    [[nodiscard]] static bool key_released(KeyCode key);
    [[nodiscard]] static bool mouse_down(MouseCode button);
    [[nodiscard]] static bool mouse_pressed(MouseCode button);
    [[nodiscard]] static bool mouse_released(MouseCode button);
    static void cursor_position(Vec2f& out);
    static void scroll_delta(Vec2f& out);
};

} // namespace oryx
