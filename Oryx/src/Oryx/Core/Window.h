#pragma once

#include "Oryx/Core/Base.h"

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

class Window
{
public:
    explicit Window(WindowDesc desc) : m_desc(std::move(desc)) {}
    virtual ~Window() = default;

    [[nodiscard]] const WindowDesc& desc() const { return m_desc; }

    [[nodiscard]] virtual bool should_close() const = 0;
    [[nodiscard]] virtual NativeWindowHandle native_handle() const = 0;

private:
    WindowDesc m_desc;
};

} // namespace oryx
