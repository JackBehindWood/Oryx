#pragma once

#include "MetalCommandQueue.h"
#include "MetalDevice.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

class MetalRHI final : public IRHI
{
public:
    MetalRHI();
    ~MetalRHI() override;

    [[nodiscard]] RHIBackend backend() const override { return RHIBackend::Metal; }
    [[nodiscard]] const RHICapabilities& capabilities() const override;

    RHIBufferPtr create_buffer(const RHIBufferDesc& desc) override;
    RHITexturePtr create_texture(const RHITextureDesc& desc) override;
    RHISamplerPtr create_sampler(const RHISamplerDesc& desc) override;
    RHIVertexShaderPtr create_vertex_shader(const RHIShaderDesc& desc) override;
    RHIPixelShaderPtr create_pixel_shader(const RHIShaderDesc& desc) override;
    RHIGraphicsPipelinePtr create_graphics_pipeline(const RHIGraphicsPipelineDesc& desc) override;
    RHIRenderTargetPtr create_render_target(const RHIRenderTargetDesc& desc) override;
    RHIViewportPtr create_viewport(const RHIViewportDesc& desc) override;
    void resize_viewport(RHIViewport& viewport, uint32_t width, uint32_t height, float scale) override;

    void submit(RHICommandList& commands) override;
    void present(RHIViewport& viewport, RHITexture* source = nullptr) override;
    void end_frame() override;
    [[nodiscard]] uint32_t frame_slot() const override { return m_queue.current_frame(); }
    void read_texture(RHITexture& texture, uint8_t* out, uint32_t out_size) override;
    void wait_idle() override;

private:
    // Declared first so it is destroyed last: it drains retired resources once the device is gone.
    RHIDeviceLease m_lease;
    metal::MetalDevice m_device;
    metal::MetalCommandQueue m_queue;
};

} // namespace oryx
