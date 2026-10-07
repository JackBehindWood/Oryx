#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Input.h"

namespace oryx
{

struct NativeWindowHandle
{
    void* native = nullptr;
    int32_t width = 0;
    int32_t height = 0;
    int32_t framebuffer_width = 0;
    int32_t framebuffer_height = 0;
    float content_scale = 1.0f;
};

struct WindowDesc
{
    std::string title = "Oryx";
    int32_t width = 1280;
    int32_t height = 720;
};

// The pointer image a window can show; the order matches the GUI's CursorShape.
enum class CursorKind : uint8_t
{
    Arrow,
    Hand,
    ResizeHorizontal,
    ResizeVertical,
    Text
};

class Window
{
public:
    explicit Window(WindowDesc desc) : m_desc(std::move(desc)) {}
    virtual ~Window() = default;

    [[nodiscard]] const WindowDesc& desc() const { return m_desc; }

    // Selects the platform backend at compile time (MacOS/GLFW with graphics, else Null); never returns null.
    [[nodiscard]] static UniquePtr<Window> create(WindowDesc desc);

    [[nodiscard]] virtual bool should_close() const = 0;
    [[nodiscard]] virtual NativeWindowHandle native_handle() const = 0;
    [[nodiscard]] virtual const IInput& input() const = 0;

    // Polls the OS, updates input() and posts events through Application::Get().post_event.
    virtual void poll_events() = 0;
    virtual void request_close() = 0;

    // Defaulted: a window without a clipboard reads empty and drops writes.
    [[nodiscard]] virtual std::string clipboard_text() const { return {}; }
    virtual void set_clipboard_text(std::string_view) {}
    // Defaulted: a window without a visible pointer ignores it.
    virtual void set_cursor_kind(CursorKind) {}

private:
    WindowDesc m_desc;
};

} // namespace oryx
