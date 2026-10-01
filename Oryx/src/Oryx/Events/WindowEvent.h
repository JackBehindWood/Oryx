#pragma once

#include "Oryx/Events/Event.h"

namespace oryx
{

class WindowCloseEvent : public Event
{
public:
    OX_EVENT_CLASS_TYPE(WindowClose)
    OX_EVENT_CLASS_CATEGORY(EventCategoryWindow)
};

class WindowResizeEvent : public Event
{
public:
    WindowResizeEvent(int32_t width, int32_t height) : m_width(width), m_height(height) {}

    [[nodiscard]] int32_t width() const { return m_width; }
    [[nodiscard]] int32_t height() const { return m_height; }

    OX_EVENT_CLASS_TYPE(WindowResize)
    OX_EVENT_CLASS_CATEGORY(EventCategoryWindow)

private:
    int32_t m_width;
    int32_t m_height;
};

} // namespace oryx
