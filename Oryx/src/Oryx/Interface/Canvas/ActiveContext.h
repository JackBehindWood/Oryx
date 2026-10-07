#pragma once

#include "Oryx/Core/Error.h"

namespace oryx
{

template<typename T>
class ContextScope;

// The active context of type T: private to the framework, readable by free-function widgets, changed only through a ContextScope. Each derived context type has its own slot.
// Main thread only.
template<typename T>
class ActiveContext
{
public:
    // Null when none is active.
    [[nodiscard]] static T* get() { return s_active; }

    // Throws Error when none is active.
    [[nodiscard]] static T& require()
    {
        if (s_active == nullptr)
        {
            throw Error("no active context", "wrap the calls in a ContextScope");
        }
        return *s_active;
    }

private:
    friend class ContextScope<T>;
    static inline T* s_active = nullptr;
};

// Makes `context` the active one for its lifetime and restores the previous one, also when unwinding.
template<typename T>
class ContextScope
{
public:
    explicit ContextScope(T& context)
        : m_previous(ActiveContext<T>::s_active)
    {
        ActiveContext<T>::s_active = &context;
    }
    ~ContextScope() { ActiveContext<T>::s_active = m_previous; }

    ContextScope(const ContextScope&) = delete;
    ContextScope& operator=(const ContextScope&) = delete;

private:
    T* m_previous;
};

} // namespace oryx
