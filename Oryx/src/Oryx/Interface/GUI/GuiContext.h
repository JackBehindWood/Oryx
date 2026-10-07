#pragma once

#include "Oryx/Interface/Canvas/ActiveContext.h"
#include "Oryx/Interface/Canvas/ImContext.h"
#include "Oryx/Interface/GUI/GuiTheme.h"

namespace oryx
{

inline constexpr uint64_t k_gui_id_seed = 0x4755'4944'5345'4544ull;

// The immediate-mode context of the developer tooling: ImContext plus the GuiTheme. Make it the active one with a ContextScope<GuiContext> to use the oryx::gui functions.
class GuiContext : public ImContext
{
public:
    GuiContext()
        : ImContext(k_gui_id_seed)
    {
    }

    using ImContext::set_theme;
    // The shared part also goes to the base context; the GUI roles stay here.
    void set_theme(const GuiTheme& theme)
    {
        ImContext::set_theme(theme);
        m_gui_theme = theme;
    }
    [[nodiscard]] const GuiTheme& gui_theme() const { return m_gui_theme; }

private:
    GuiTheme m_gui_theme;
};

} // namespace oryx
