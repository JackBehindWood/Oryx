#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// A dialog that blocks everything beneath it: a dimmed backdrop and a centred box on the popup layer. Escape cancels; a press outside does not.
// A frame with a modal open reports ImContext::popup_open(), which is what the owner uses to keep the pointer and keyboard away from the world.
namespace oryx::gui
{

enum class ModalChoice : uint8_t
{
    None,
    Confirmed,
    Cancelled
};

void open_modal(std::string_view name);
void close_modal(std::string_view name);
// From inside the modal's own contents (not inside a popup nested in it).
void close_current_modal();
// Returns whether the modal is open; fill it and call end_modal only when true.
[[nodiscard]] bool begin_modal(std::string_view name, const WidgetOptions& options = {});
void end_modal();

// The common dialog in one call, drawn every frame once opened with open_modal: Confirmed or Cancelled (a click or Escape) on the frame it is answered, None otherwise.
[[nodiscard]] ModalChoice confirm(std::string_view name, std::string_view message, std::string_view confirm_label, std::string_view cancel_label);

class ModalScope
{
public:
    explicit ModalScope(std::string_view name, const WidgetOptions& options = {})
        : m_open(begin_modal(name, options))
    {
    }
    ~ModalScope()
    {
        if (m_open)
        {
            end_modal();
        }
    }

    ModalScope(const ModalScope&) = delete;
    ModalScope& operator=(const ModalScope&) = delete;

    [[nodiscard]] bool open() const { return m_open; }

private:
    bool m_open;
};

} // namespace oryx::gui
