#pragma once

#include "Oryx/Core/PolledInput.h"
#include "Oryx/Core/Window.h"

struct GLFWwindow;
struct GLFWcursor;

namespace oryx
{

// GLFW + Cocoa. Created without a client API: the RHI attaches its own surface to the NSWindow/NSView.
class MacOSWindow final : public Window
{
public:
    explicit MacOSWindow(WindowDesc desc);
    ~MacOSWindow() override;

    MacOSWindow(const MacOSWindow&) = delete;
    MacOSWindow& operator=(const MacOSWindow&) = delete;

    [[nodiscard]] bool should_close() const override;
    [[nodiscard]] NativeWindowHandle native_handle() const override;
    [[nodiscard]] const IInput& input() const override { return m_input; }

    void poll_events() override;
    void request_close() override;
    [[nodiscard]] std::string clipboard_text() const override;
    void set_clipboard_text(std::string_view text) override;
    void set_cursor_kind(CursorKind kind) override;

private:
    GLFWwindow* m_window = nullptr;
    PolledInput m_input;
    GLFWcursor* m_cursors[5] = {};
    CursorKind m_cursor_kind = CursorKind::Arrow;
};

} // namespace oryx
