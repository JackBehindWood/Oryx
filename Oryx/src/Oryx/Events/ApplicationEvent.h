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

} // namespace oryx
