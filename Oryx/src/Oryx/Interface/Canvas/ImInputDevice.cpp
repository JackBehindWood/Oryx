#include "oxpch.h"
#include "Oryx/Interface/Canvas/ImInputDevice.h"

namespace oryx
{

namespace
{

struct KeyMapping
{
    KeyCode code;
    ImKey key;
};

constexpr KeyMapping k_key_mappings[] = {
    { KeyCode::Escape, ImKey::Escape },
    { KeyCode::Enter, ImKey::Enter },
    { KeyCode::Tab, ImKey::Tab },
    { KeyCode::Backspace, ImKey::Backspace },
    { KeyCode::Delete, ImKey::Delete },
    { KeyCode::Left, ImKey::Left },
    { KeyCode::Right, ImKey::Right },
    { KeyCode::Up, ImKey::Up },
    { KeyCode::Down, ImKey::Down },
    { KeyCode::Home, ImKey::Home },
    { KeyCode::End, ImKey::End },
    { KeyCode::PageUp, ImKey::PageUp },
    { KeyCode::PageDown, ImKey::PageDown },
};

bool either_down(const IInput& input, KeyCode left, KeyCode right)
{
    return input.key_down(left) || input.key_down(right);
}

} // namespace

ImInput make_im_input(const IInput& input, uint32_t surface, const Vec2f& surface_size, float scale, float delta_time)
{
    ImInput result;
    result.surface = surface;
    result.surface_size = surface_size;
    result.scale = scale;
    result.delta_time = delta_time;

    Vec2f cursor(0.0f, 0.0f);
    input.cursor_position(cursor);
    result.pointer.position = cursor;
    result.pointer.valid = cursor[0] >= 0.0f && cursor[1] >= 0.0f && cursor[0] < surface_size[0] && cursor[1] < surface_size[1];
    for (uint32_t button = 0; button < k_im_button_count; ++button)
    {
        const MouseCode code = static_cast<MouseCode>(button);
        result.pointer.buttons[button] = { input.mouse_down(code), input.mouse_pressed(code), input.mouse_released(code) };
    }

    input.scroll_delta(result.wheel);

    for (const KeyMapping& mapping : k_key_mappings)
    {
        result.keys.down |= input.key_down(mapping.code) ? im_key_bit(mapping.key) : 0u;
        result.keys.pressed |= input.key_pressed(mapping.code) ? im_key_bit(mapping.key) : 0u;
    }
    result.keys.ctrl = either_down(input, KeyCode::LeftControl, KeyCode::RightControl);
    result.keys.shift = either_down(input, KeyCode::LeftShift, KeyCode::RightShift);
    result.keys.alt = either_down(input, KeyCode::LeftAlt, KeyCode::RightAlt);
    result.keys.super = either_down(input, KeyCode::LeftSuper, KeyCode::RightSuper);
    return result;
}

} // namespace oryx
