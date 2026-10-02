#pragma once

#include "Oryx/Core/Base.h"

namespace MTL
{
class Device;
}

namespace CA
{
class MetalLayer;
}

namespace oryx::metal
{

struct MetalSurfaceDesc
{
    bool vsync = true;
};

// Child view of the window's content view that hosts a CAMetalLayer; AppKit objects stay behind the pimpl.
// Every method is main-thread only and throws Error instead of letting an Objective-C exception escape.
class MetalSurface
{
public:
    MetalSurface(void* ns_window, MTL::Device* device, const MetalSurfaceDesc& desc);
    ~MetalSurface();
    MetalSurface(const MetalSurface&) = delete;
    MetalSurface& operator=(const MetalSurface&) = delete;

    [[nodiscard]] CA::MetalLayer* layer() const;
    [[nodiscard]] uint32_t width_px() const;
    [[nodiscard]] uint32_t height_px() const;
    [[nodiscard]] float content_scale() const;
    [[nodiscard]] bool is_visible() const;
    void set_vsync(bool vsync);
    void set_colourspace_srgb();
    void resize(uint32_t width_px, uint32_t height_px, float scale);

private:
    struct Impl;
    UniquePtr<Impl> m_impl;
};

} // namespace oryx::metal
