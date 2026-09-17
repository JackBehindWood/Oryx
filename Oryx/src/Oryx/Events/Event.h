#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// Events are dispatched synchronously: when one occurs it is handed
// straight to Application::post_event() and must be dealt with then and
// there. EventType only grows a value once a concrete Event subclass
// actually needs it (see ARCHITECTURE.md §3.6) - AppTick (AppTickEvent,
// Oryx/Events/ApplicationEvent.h) is the first.
enum class EventType
{
    None = 0,
    AppTick
};

enum EventCategory
{
    None = 0,
    EventCategoryApplication = BIT(0)
};

#define OX_EVENT_CLASS_TYPE(type) static ::oryx::EventType static_type() { return ::oryx::EventType::type; }\
    virtual ::oryx::EventType event_type() const override { return static_type(); }\
    virtual const char* name() const override { return #type; }

#define OX_EVENT_CLASS_CATEGORY(category) virtual int32_t category_flags() const override { return category; }

class Event
{
public:
    virtual ~Event() = default;

    bool handled = false;

    virtual EventType event_type() const = 0;
    virtual const char* name() const = 0;
    virtual int32_t category_flags() const = 0;
    virtual std::string to_string() const { return name(); }

    bool is_in_category(EventCategory category) const
    {
        return category_flags() & category;
    }
};

class EventDispatcher
{
public:
    explicit EventDispatcher(Event& event)
        : m_event(event)
    {
    }

    // F is deduced by the compiler.
    template<typename T, typename F>
    bool dispatch(const F& func)
    {
        if (m_event.event_type() == T::static_type())
        {
            m_event.handled |= func(static_cast<T&>(m_event));
            return true;
        }
        return false;
    }

private:
    Event& m_event;
};

inline std::ostream& operator<<(std::ostream& os, const Event& e)
{
    return os << e.to_string();
}

} // namespace oryx
