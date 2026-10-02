#pragma once

#include "MetalApi.h"
#include "MetalDevice.h"
#include "MetalInterop.h"
#include "MetalResources.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"

namespace oryx::metal
{

// With a native window the swapchain is a CAMetalLayer in a child view; without one it is a single offscreen texture, so the
// viewport contract (and present) also runs headless.
class MetalViewport final : public RHIViewport
{
public:
    MetalViewport(const MetalDevice& device, const RHIViewportDesc& desc);

    void resize(uint32_t width, uint32_t height, float scale);
    [[nodiscard]] uint32_t width() const override;
    [[nodiscard]] uint32_t height() const override;
    [[nodiscard]] RHIFormat format() const override { return m_format; }
    [[nodiscard]] RHIRenderTargetPtr acquire_back_buffer() override;

    [[nodiscard]] bool has_drawable() const { return m_back_buffer && m_back_buffer->drawable() != nullptr; }
    // Queues the acquired drawable for presentation and forgets it; offscreen viewports keep their texture.
    void encode_present(MTL::CommandBuffer& commands);

private:
    const MetalDevice& m_device;
    RHIFormat m_format;
    uint32_t m_width;
    uint32_t m_height;
    UniquePtr<MetalSurface> m_surface;
    Ref<MetalRenderTarget> m_back_buffer;
};

} // namespace oryx::metal
