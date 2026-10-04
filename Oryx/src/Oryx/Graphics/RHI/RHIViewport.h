#pragma once

#include "Oryx/Graphics/RHI/RHIFormat.h"
#include "Oryx/Graphics/RHI/RHIRenderTarget.h"

namespace oryx
{

struct RHIViewportDesc
{
    // Opaque platform surface handle (e.g. NSWindow*); the RHI never sees a window class.
    void* native_window = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    float scale = 1.0f;
    RHIFormat format = RHIFormat::BGRA8Unorm;
    bool vsync = true;
};

class RHIViewport : public RHIResource
{
public:
    [[nodiscard]] virtual uint32_t width() const = 0;
    [[nodiscard]] virtual uint32_t height() const = 0;
    [[nodiscard]] virtual RHIFormat format() const = 0;
    // Null when the surface is zero-sized, hidden or has no drawable available.
    [[nodiscard]] virtual RHIRenderTargetPtr acquire_back_buffer() = 0;
    // Releases an acquired back buffer that will not be presented (e.g. after a failed submit).
    virtual void discard_back_buffer() = 0;

protected:
    RHIViewport() = default;
};

using RHIViewportPtr = Ref<RHIViewport>;

// The shared contract for IRHI::present's source: sampled, and exactly the viewport's size and format. Throws Error.
void rhi_validate_present_source(const RHIViewport& viewport, const RHITexture& source);

} // namespace oryx
