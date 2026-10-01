#pragma once

#include "Oryx/Core/KeyCode.h"
#include "Oryx/Events/Event.h"

namespace oryx
{

class KeyEvent : public Event
{
public:
    [[nodiscard]] KeyCode key() const { return m_key; }

    OX_EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)

protected:
    explicit KeyEvent(KeyCode key) : m_key(key) {}

private:
    KeyCode m_key;
};

class KeyPressedEvent : public KeyEvent
{
public:
    explicit KeyPressedEvent(KeyCode key) : KeyEvent(key) {}

    OX_EVENT_CLASS_TYPE(KeyPressed)
};

class KeyReleasedEvent : public KeyEvent
{
public:
    explicit KeyReleasedEvent(KeyCode key) : KeyEvent(key) {}

    OX_EVENT_CLASS_TYPE(KeyReleased)
};

} // namespace oryx
