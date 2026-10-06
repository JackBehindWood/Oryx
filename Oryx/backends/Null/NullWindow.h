#pragma once

#include "Oryx/Core/PolledInput.h"
#include "Oryx/Core/Window.h"

namespace oryx
{

// Headless window: no display, no GPU. The inject_* methods stand in for the OS and post the matching events.
class NullWindow final : public Window
{
public:
    explicit NullWindow(WindowDesc desc);

    [[nodiscard]] bool should_close() const override { return m_should_close; }
    [[nodiscard]] NativeWindowHandle native_handle() const override;
    [[nodiscard]] const IInput& input() const override { return m_input; }

    void poll_events() override { m_input.begin_frame(); }
    void request_close() override { m_should_close = true; }

    void inject_key(KeyCode key, bool down);
    void inject_mouse_button(MouseCode button, bool down);
    void inject_cursor(float x, float y);
    void inject_scroll(float dx, float dy);
    void inject_resize(int32_t width, int32_t height);
    // A display change (e.g. dragging the window to a HiDPI screen): logical size stays, the framebuffer becomes size * scale.
    void inject_scale(float scale);
    void inject_focus(bool focused);
    void inject_close();

private:
    PolledInput m_input;
    int32_t m_width;
    int32_t m_height;
    float m_scale = 1.0f;
    bool m_should_close = false;
};

} // namespace oryx
