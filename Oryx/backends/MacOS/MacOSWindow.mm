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
        if (action == GLFW_REPEAT)
        {
            return;
        }
        KeyCode code = static_cast<KeyCode>(key);
        bool down = action == GLFW_PRESS;
        owner(window).m_input.set_key(code, down);
        if (down)
        {
            post(KeyPressedEvent(code));
        }
        else
        {
            post(KeyReleasedEvent(code));
        }
    });

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
    glfwDestroyWindow(m_window);
    if (--g_glfw_users == 0)
    {
        glfwTerminate();
    }
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
