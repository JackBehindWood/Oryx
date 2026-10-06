#pragma once

#include "Oryx/Core/PolledInput.h"
#include "Oryx/Core/Window.h"

struct GLFWwindow;

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

private:
    GLFWwindow* m_window = nullptr;
    PolledInput m_input;
};

} // namespace oryx
