#pragma once

#include "Oryx/Events/Event.h"

namespace oryx
{

class ReloadScriptsEvent : public Event
{
public:
    ReloadScriptsEvent() = default;

    OX_EVENT_CLASS_TYPE(ReloadScripts)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)
};

} // namespace oryx
