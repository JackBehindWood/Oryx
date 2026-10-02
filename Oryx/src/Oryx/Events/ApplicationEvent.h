#pragma once

#include "Oryx/Events/Event.h"

namespace oryx
{

class AppTickEvent : public Event
{
public:
    AppTickEvent() = default;

    OX_EVENT_CLASS_TYPE(AppTick)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

class ApplicationCloseEvent : public Event
{
public:
    explicit ApplicationCloseEvent(int32_t exit_code) : m_exit_code(exit_code) {}

    [[nodiscard]] int32_t exit_code() const { return m_exit_code; }

    OX_EVENT_CLASS_TYPE(ApplicationClose)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    int32_t m_exit_code;
};

} // namespace oryx
