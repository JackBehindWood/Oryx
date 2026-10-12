#pragma once

#include "Oryx/Interface/GUI/GuiContext.h"

// The begin/end rule of every scoped widget, as in ImGui: `begin_x` returns whether the widget is open and `end_x` is called only when it did; widgets that always open say so and always end.
// A Scope calls End::end() on destruction only when it was opened; an AlwaysScope always does. A mismatched, missing or unclosed end throws Error (see ImContext::push_scope), and unwinding from another error never throws a second one.
namespace oryx::gui
{

namespace detail
{

// During unwinding a failing end is swallowed: the frame is abandoned by the layer anyway and a second exception would terminate.
template<typename End>
void finish(int32_t uncaught_at_begin)
{
    if (std::uncaught_exceptions() > uncaught_at_begin)
    {
        try
        {
            End::end();
        }
        catch (...)
        {
        }
        return;
    }
    End::end();
}

} // namespace detail

template<typename End>
class Scope
{
public:
    explicit Scope(bool open)
        : m_open(open)
        , m_uncaught(std::uncaught_exceptions())
    {
    }
    ~Scope() noexcept(false)
    {
        if (m_open)
        {
            detail::finish<End>(m_uncaught);
        }
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    [[nodiscard]] bool open() const { return m_open; }
    explicit operator bool() const { return m_open; }

private:
    bool m_open;
    int32_t m_uncaught;
};

template<typename End>
class AlwaysScope
{
public:
    AlwaysScope()
        : m_uncaught(std::uncaught_exceptions())
    {
    }
    ~AlwaysScope() noexcept(false) { detail::finish<End>(m_uncaught); }

    AlwaysScope(const AlwaysScope&) = delete;
    AlwaysScope& operator=(const AlwaysScope&) = delete;

private:
    int32_t m_uncaught;
};

// A scope kind that needs no state of its own but a place in the stack, so a wrong end_* is caught; tagged by its End policy.
template<typename End>
struct ScopeMarker
{
};

// Call from begin_x when it opens and from end_x; `end` names the end function in the error.
template<typename End>
void open_marker(const char* name)
{
    ActiveContext<GuiContext>::require().template push_scope<ScopeMarker<End>>(name);
}

template<typename End>
void close_marker(const char* end)
{
    ActiveContext<GuiContext>::require().template pop_scope<ScopeMarker<End>>(end);
}

} // namespace oryx::gui
