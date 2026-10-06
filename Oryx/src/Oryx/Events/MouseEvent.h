#pragma once

#include "Oryx/Core/MouseCode.h"
#include "Oryx/Events/Event.h"

namespace oryx
{

class MouseMovedEvent : public Event
{
public:
    MouseMovedEvent(float x, float y) : m_x(x), m_y(y) {}

    [[nodiscard]] float x() const { return m_x; }
    [[nodiscard]] float y() const { return m_y; }

    OX_EVENT_CLASS_TYPE(MouseMoved)
    OX_EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
    float m_x;
    float m_y;
};

class MouseButtonPressedEvent : public Event
{
public:
    explicit MouseButtonPressedEvent(MouseCode button) : m_button(button) {}

    [[nodiscard]] MouseCode button() const { return m_button; }

    OX_EVENT_CLASS_TYPE(MouseButtonPressed)
    OX_EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
    MouseCode m_button;
};

class MouseButtonReleasedEvent : public Event
{
public:
    explicit MouseButtonReleasedEvent(MouseCode button) : m_button(button) {}

    [[nodiscard]] MouseCode button() const { return m_button; }

    OX_EVENT_CLASS_TYPE(MouseButtonReleased)
    OX_EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
    MouseCode m_button;
};

class MouseScrolledEvent : public Event
{
public:
    MouseScrolledEvent(float dx, float dy) : m_dx(dx), m_dy(dy) {}

    [[nodiscard]] float dx() const { return m_dx; }
    [[nodiscard]] float dy() const { return m_dy; }

    OX_EVENT_CLASS_TYPE(MouseScrolled)
    OX_EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

private:
    float m_dx;
    float m_dy;
};

} // namespace oryx
