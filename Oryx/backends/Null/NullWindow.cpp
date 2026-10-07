#include "oxpch.h"
#include "NullWindow.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Core/Utf8.h"
#include "Oryx/Events/KeyEvent.h"
#include "Oryx/Events/MouseEvent.h"
#include "Oryx/Events/WindowEvent.h"

namespace oryx
{

namespace
{

void post(Event&& event)
{
    Application::Get().post_event(event);
}

int32_t scaled(int32_t points, float scale)
{
    return static_cast<int32_t>(std::lround(static_cast<float>(points) * scale));
}

} // namespace

NullWindow::NullWindow(WindowDesc desc)
    : Window(std::move(desc))
    , m_width(this->desc().width)
    , m_height(this->desc().height)
{
}

NativeWindowHandle NullWindow::native_handle() const
{
    return { nullptr, m_width, m_height, scaled(m_width, m_scale), scaled(m_height, m_scale), m_scale };
}

void NullWindow::inject_key(KeyCode key, bool down)
{
    m_input.set_key(key, down);
    if (down)
    {
        post(KeyPressedEvent(key));
    }
    else
    {
        post(KeyReleasedEvent(key));
    }
}

void NullWindow::inject_text(std::string_view utf8)
{
    size_t index = 0;
    while (index < utf8.size())
    {
        m_input.add_text(decode_utf8(utf8, index));
    }
}

void NullWindow::inject_key_repeat(KeyCode key)
{
    m_input.set_key_repeat(key);
}

void NullWindow::inject_paste(std::string_view text)
{
    m_input.set_paste_text(text);
}

void NullWindow::inject_mouse_button(MouseCode button, bool down)
{
    m_input.set_mouse_button(button, down);
    if (down)
    {
        post(MouseButtonPressedEvent(button));
    }
    else
    {
        post(MouseButtonReleasedEvent(button));
    }
}

void NullWindow::inject_cursor(float x, float y)
{
    m_input.set_cursor(x, y);
    post(MouseMovedEvent(x, y));
}

void NullWindow::inject_scroll(float dx, float dy)
{
    m_input.add_scroll(dx, dy);
    post(MouseScrolledEvent(dx, dy));
}

void NullWindow::inject_resize(int32_t width, int32_t height)
{
    m_width = width;
    m_height = height;
    post(WindowResizeEvent(width, height));
}

void NullWindow::inject_scale(float scale)
{
    if (!(scale > 0.0f))
    {
        throw Error("NullWindow scale must be positive");
    }
    m_scale = scale;
    post(WindowResizeEvent(m_width, m_height));
}

void NullWindow::inject_focus(bool focused)
{
    post(WindowFocusEvent(focused));
}

void NullWindow::inject_close()
{
    m_should_close = true;
    post(WindowCloseEvent());
}

} // namespace oryx
