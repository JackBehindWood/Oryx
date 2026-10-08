#include "oxpch.h"
#include "Oryx/Core/InputRouter.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

constexpr MouseCode k_buttons[] = { MouseCode::Left, MouseCode::Right, MouseCode::Middle };
constexpr float k_offscreen = -1.0e6f;

void check_view(ViewId view)
{
    if (view >= k_max_views)
    {
        throw Error("View id out of range", "views are 0.." + std::to_string(k_max_views - 1));
    }
}

} // namespace

InputRouter::InputRouter()
    : m_interface(*this, false, k_main_view)
{
    m_world.reserve(k_max_views);
    for (ViewId view = 0; view < k_max_views; ++view)
    {
        m_world.emplace_back(*this, true, view);
    }
}

void InputRouter::begin_frame(const IInput& input, const Vec2f& surface)
{
    m_input = &input;
    for (uint32_t view = 0; view < k_max_views; ++view)
    {
        m_defined[view] = false;
    }
    m_regions[k_main_view] = { { 0.0f, 0.0f }, surface };
    m_defined[k_main_view] = true;
    m_pointer_claimed = false;
    m_keyboard_claimed = false;
    m_hover_view = k_max_views;

    bool active = false;
    for (MouseCode button : k_buttons)
    {
        active = active || input.mouse_down(button) || input.mouse_released(button);
    }
    if (!active)
    {
        m_owner = PointerOwner::None;
    }
}

void InputRouter::set_view(ViewId view, const ViewRegion& region)
{
    check_view(view);
    m_regions[view] = region;
    m_defined[view] = true;
}

ViewId InputRouter::view_under_cursor() const
{
    Vec2f cursor;
    m_input->cursor_position(cursor);
    for (ViewId view = 0; view < k_max_views; ++view)
    {
        if (m_defined[view] && contains(m_regions[view], cursor))
        {
            return view;
        }
    }
    return k_max_views;
}

void InputRouter::begin_world()
{
    if (m_input == nullptr)
    {
        return;
    }
    m_hover_view = view_under_cursor();
    if (m_owner != PointerOwner::None)
    {
        return;
    }
    bool pressed = false;
    for (MouseCode button : k_buttons)
    {
        pressed = pressed || m_input->mouse_pressed(button);
    }
    if (!pressed)
    {
        return;
    }
    if (m_pointer_claimed || m_hover_view == k_max_views)
    {
        m_owner = PointerOwner::Interface;
    }
    else
    {
        m_owner = PointerOwner::World;
        m_owner_view = m_hover_view;
    }
}

const IInput& InputRouter::world_input(ViewId view) const
{
    check_view(view);
    return m_world[view];
}

const ViewRegion& InputRouter::view(ViewId view) const
{
    check_view(view);
    return m_regions[view];
}

bool InputRouter::Filtered::keys_visible() const
{
    return m_router.m_input != nullptr && (!m_world || !m_router.m_keyboard_claimed);
}

bool InputRouter::Filtered::buttons_visible() const
{
    if (m_router.m_input == nullptr)
    {
        return false;
    }
    if (!m_world)
    {
        return m_router.m_owner != PointerOwner::World;
    }
    return m_router.m_owner == PointerOwner::World && m_router.m_owner_view == m_view;
}

bool InputRouter::Filtered::hover_visible() const
{
    if (m_router.m_input == nullptr)
    {
        return false;
    }
    if (!m_world)
    {
        return m_router.m_owner != PointerOwner::World;
    }
    if (m_router.m_owner == PointerOwner::World)
    {
        return m_router.m_owner_view == m_view;
    }
    return m_router.m_owner == PointerOwner::None && !m_router.m_pointer_claimed && m_router.m_hover_view == m_view;
}

bool InputRouter::Filtered::key_down(KeyCode key) const
{
    return keys_visible() && m_router.m_input->key_down(key);
}

bool InputRouter::Filtered::key_pressed(KeyCode key) const
{
    return keys_visible() && m_router.m_input->key_pressed(key);
}

bool InputRouter::Filtered::key_released(KeyCode key) const
{
    return keys_visible() && m_router.m_input->key_released(key);
}

bool InputRouter::Filtered::mouse_down(MouseCode button) const
{
    return buttons_visible() && m_router.m_input->mouse_down(button);
}

bool InputRouter::Filtered::mouse_pressed(MouseCode button) const
{
    return buttons_visible() && m_router.m_input->mouse_pressed(button);
}

bool InputRouter::Filtered::mouse_released(MouseCode button) const
{
    return buttons_visible() && m_router.m_input->mouse_released(button);
}

void InputRouter::Filtered::cursor_position(Vec2f& out) const
{
    if (!hover_visible() || (m_world && !m_router.m_defined[m_view]))
    {
        out = Vec2f(k_offscreen, k_offscreen);
        return;
    }
    m_router.m_input->cursor_position(out);
    if (m_world)
    {
        out = out - m_router.m_regions[m_view].min;
    }
}

void InputRouter::Filtered::scroll_delta(Vec2f& out) const
{
    if (!hover_visible())
    {
        out = Vec2f(0.0f, 0.0f);
        return;
    }
    m_router.m_input->scroll_delta(out);
}

std::string_view InputRouter::Filtered::typed_text() const
{
    return keys_visible() ? m_router.m_input->typed_text() : std::string_view{};
}

bool InputRouter::Filtered::key_repeated(KeyCode key) const
{
    return keys_visible() && m_router.m_input->key_repeated(key);
}

std::string_view InputRouter::Filtered::paste_text() const
{
    return keys_visible() ? m_router.m_input->paste_text() : std::string_view{};
}

} // namespace oryx
