#include "oxpch.h"
#include "Oryx/Core/PolledInput.h"

namespace oryx
{

namespace
{

template<typename Array, typename Code>
auto* slot(Array& array, Code code)
{
    int32_t index = static_cast<int32_t>(code);
    bool in_range = index >= 0 && index < static_cast<int32_t>(array.size());
    return in_range ? &array[static_cast<size_t>(index)] : nullptr;
}

} // namespace

void PolledInput::apply(Edge& edge, bool down)
{
    if (edge.down == down)
    {
        return;
    }
    edge.down = down;
    edge.pressed = down;
    edge.released = !down;
}

void PolledInput::begin_frame()
{
    for (Edge& key : m_keys)
    {
        key.pressed = false;
        key.released = false;
    }
    for (Edge& button : m_buttons)
    {
        button.pressed = false;
        button.released = false;
    }
    m_scroll_x = 0.0f;
    m_scroll_y = 0.0f;
}

void PolledInput::set_key(KeyCode key, bool down)
{
    if (Edge* edge = slot(m_keys, key))
    {
        apply(*edge, down);
    }
}

void PolledInput::set_mouse_button(MouseCode button, bool down)
{
    if (Edge* edge = slot(m_buttons, button))
    {
        apply(*edge, down);
    }
}

void PolledInput::set_cursor(float x, float y)
{
    m_cursor_x = x;
    m_cursor_y = y;
}

void PolledInput::add_scroll(float dx, float dy)
{
    m_scroll_x += dx;
    m_scroll_y += dy;
}

bool PolledInput::key_down(KeyCode key) const
{
    const Edge* edge = slot(m_keys, key);
    return edge && edge->down;
}

bool PolledInput::key_pressed(KeyCode key) const
{
    const Edge* edge = slot(m_keys, key);
    return edge && edge->pressed;
}

bool PolledInput::key_released(KeyCode key) const
{
    const Edge* edge = slot(m_keys, key);
    return edge && edge->released;
}

bool PolledInput::mouse_down(MouseCode button) const
{
    const Edge* edge = slot(m_buttons, button);
    return edge && edge->down;
}

bool PolledInput::mouse_pressed(MouseCode button) const
{
    const Edge* edge = slot(m_buttons, button);
    return edge && edge->pressed;
}

bool PolledInput::mouse_released(MouseCode button) const
{
    const Edge* edge = slot(m_buttons, button);
    return edge && edge->released;
}

void PolledInput::cursor_position(Vec2f& out) const
{
    out = Vec2f(m_cursor_x, m_cursor_y);
}

void PolledInput::scroll_delta(Vec2f& out) const
{
    out = Vec2f(m_scroll_x, m_scroll_y);
}

} // namespace oryx
