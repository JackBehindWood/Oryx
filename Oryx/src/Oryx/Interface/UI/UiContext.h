#pragma once

#include "Oryx/Interface/Canvas/ActiveContext.h"
#include "Oryx/Interface/Canvas/ImContext.h"
#include "Oryx/Interface/UI/UiTheme.h"

namespace oryx
{

// The immediate-mode context of the player-facing UI: ImContext plus the UiTheme. Make it the active one with a ContextScope<UiContext> to use the oryx::ui functions.
class UiContext : public ImContext
{
public:
    using ImContext::set_theme;
    // The shared part also goes to the base context; the UI roles stay here.
    void set_theme(const UiTheme& theme)
    {
        ImContext::set_theme(theme);
        m_ui_theme = theme;
    }
    [[nodiscard]] const UiTheme& ui_theme() const { return m_ui_theme; }

private:
    UiTheme m_ui_theme;
};

} // namespace oryx
