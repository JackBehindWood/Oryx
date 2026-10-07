#include "oxpch.h"
#include "MacOSWindow.h"

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Events/KeyEvent.h"
#include "Oryx/Events/MouseEvent.h"
#include "Oryx/Events/WindowEvent.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace oryx
{

namespace
{

int32_t g_glfw_users = 0;

void glfw_error(int code, const char* description)
{
    OX_CORE_ERROR("[glfw] {} ({})", description, code);
}

void post(Event&& event)
{
    Application::Get().post_event(event);
}

MacOSWindow& owner(GLFWwindow* window)
{
    return *static_cast<MacOSWindow*>(glfwGetWindowUserPointer(window));
}

} // namespace

MacOSWindow::MacOSWindow(WindowDesc desc)
    : Window(std::move(desc))
{
    if (g_glfw_users == 0)
    {
        glfwSetErrorCallback(glfw_error);
        if (!glfwInit())
        {
            throw Error("Failed to initialise GLFW");
        }
    }
    ++g_glfw_users;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(this->desc().width, this->desc().height, this->desc().title.c_str(), nullptr, nullptr);
    if (!m_window)
    {
        if (--g_glfw_users == 0)
        {
            glfwTerminate();
        }
        throw Error("Failed to create window", this->desc().title);
    }

    glfwSetWindowUserPointer(m_window, this);

    glfwSetWindowCloseCallback(m_window, [](GLFWwindow*) { post(WindowCloseEvent()); });
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow*, int width, int height) { post(WindowResizeEvent(width, height)); });
    glfwSetWindowFocusCallback(m_window, [](GLFWwindow*, int focused) { post(WindowFocusEvent(focused == GLFW_TRUE)); });

    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int, int action, int)
    {
        KeyCode code = static_cast<KeyCode>(key);
        if (action == GLFW_REPEAT)
        {
            owner(window).m_input.set_key_repeat(code);
            return;
        }
        bool down = action == GLFW_PRESS;
        owner(window).m_input.set_key(code, down);
        const bool paste = down && code == KeyCode::V && (glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
        if (paste)
        {
            const char* clip = glfwGetClipboardString(window);
            owner(window).m_input.set_paste_text(clip != nullptr ? std::string_view(clip) : std::string_view());
        }
        if (down)
        {
            post(KeyPressedEvent(code));
        }
        else
        {
            post(KeyReleasedEvent(code));
        }
    });

    glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int codepoint) { owner(window).m_input.add_text(codepoint); });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int)
    {
        MouseCode code = static_cast<MouseCode>(button);
        bool down = action == GLFW_PRESS;
        owner(window).m_input.set_mouse_button(code, down);
        if (down)
        {
            post(MouseButtonPressedEvent(code));
        }
        else
        {
            post(MouseButtonReleasedEvent(code));
        }
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y)
    {
        owner(window).m_input.set_cursor(static_cast<float>(x), static_cast<float>(y));
        post(MouseMovedEvent(static_cast<float>(x), static_cast<float>(y)));
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double dx, double dy)
    {
        owner(window).m_input.add_scroll(static_cast<float>(dx), static_cast<float>(dy));
        post(MouseScrolledEvent(static_cast<float>(dx), static_cast<float>(dy)));
    });
}

MacOSWindow::~MacOSWindow()
{
    for (GLFWcursor* cursor : m_cursors)
    {
        if (cursor != nullptr)
        {
            glfwDestroyCursor(cursor);
        }
    }
    glfwDestroyWindow(m_window);
    if (--g_glfw_users == 0)
    {
        glfwTerminate();
    }
}

void MacOSWindow::set_cursor_kind(CursorKind kind)
{
    static constexpr int k_shapes[5] = { GLFW_ARROW_CURSOR, GLFW_POINTING_HAND_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR, GLFW_IBEAM_CURSOR };
    if (kind == m_cursor_kind)
    {
        return;
    }
    GLFWcursor*& cursor = m_cursors[static_cast<size_t>(kind)];
    if (cursor == nullptr && kind != CursorKind::Arrow)
    {
        cursor = glfwCreateStandardCursor(k_shapes[static_cast<size_t>(kind)]);
    }
    glfwSetCursor(m_window, cursor);
    m_cursor_kind = kind;
}

std::string MacOSWindow::clipboard_text() const
{
    const char* clip = glfwGetClipboardString(m_window);
    return clip != nullptr ? std::string(clip) : std::string();
}

void MacOSWindow::set_clipboard_text(std::string_view text)
{
    const std::string copy(text);
    glfwSetClipboardString(m_window, copy.c_str());
}

bool MacOSWindow::should_close() const
{
    return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

NativeWindowHandle MacOSWindow::native_handle() const
{
    NativeWindowHandle handle;
    handle.native = (__bridge void*)glfwGetCocoaWindow(m_window);
    glfwGetWindowSize(m_window, &handle.width, &handle.height);
    glfwGetFramebufferSize(m_window, &handle.framebuffer_width, &handle.framebuffer_height);
    float scale_y = 1.0f;
    glfwGetWindowContentScale(m_window, &handle.content_scale, &scale_y);
    return handle;
}

void MacOSWindow::poll_events()
{
    m_input.begin_frame();
    glfwPollEvents();
}

void MacOSWindow::request_close()
{
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

} // namespace oryx
