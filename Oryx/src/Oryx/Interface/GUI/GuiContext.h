#pragma once

#include "Oryx/Interface/Canvas/ActiveContext.h"
#include "Oryx/Interface/Canvas/ImContext.h"
#include "Oryx/Interface/GUI/GuiTheme.h"

namespace oryx
{

inline constexpr uint64_t k_gui_id_seed = 0x4755'4944'5345'4544ull;

inline constexpr uint32_t k_max_toasts = 4;
inline constexpr size_t k_max_toast_text = 96;

// A message with the seconds it has left; the text is cut to fit.
struct GuiToast
{
    char text[k_max_toast_text] = {};
    float remaining = 0.0f;
};

static_assert(std::is_trivially_copyable_v<GuiToast> && std::is_standard_layout_v<GuiToast>);

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

    // A picked menu item asks every open menu to close; they do so in the next frame.
    void request_menu_close() { m_menu_close_frame = frame(); }
    [[nodiscard]] bool menu_close_requested() const { return m_menu_close_frame != 0 && m_menu_close_frame + 1 == frame(); }

    // Queues a message (the oldest is replaced when all slots are taken); gui::show_toasts draws and ages them.
    void add_toast(std::string_view text, float seconds)
    {
        uint32_t slot = 0;
        for (uint32_t index = 0; index < k_max_toasts; ++index)
        {
            slot = m_toasts[index].remaining < m_toasts[slot].remaining ? index : slot;
        }
        const size_t length = math::min(text.size(), k_max_toast_text - 1);
        std::memcpy(m_toasts[slot].text, text.data(), length);
        m_toasts[slot].text[length] = '\0';
        m_toasts[slot].remaining = seconds;
    }
    [[nodiscard]] std::span<GuiToast> toasts() { return m_toasts; }

private:
    GuiTheme m_gui_theme;
    std::array<GuiToast, k_max_toasts> m_toasts;
    uint64_t m_menu_close_frame = 0;
};

} // namespace oryx
