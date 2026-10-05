#pragma once

#include "Oryx/Graphics/RHI/RHIBackend.h"
#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHICapabilities.h"
#include "Oryx/Graphics/RHI/RHICommandList.h"
#include "Oryx/Graphics/RHI/RHIPipeline.h"
#include "Oryx/Graphics/RHI/RHIRenderTarget.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHIShader.h"
#include "Oryx/Graphics/RHI/RHITexture.h"
#include "Oryx/Graphics/RHI/RHIViewport.h"

namespace oryx
{

// Owned, never global. create_*, submit, present, end_frame and acquire_back_buffer are application-thread only; Ref copy/release is thread-safe.
// The device must outlive every resource it created. A resource belongs to the device that created it; mixing devices is undefined.
class IRHI
{
public:
    virtual ~IRHI() = default;

    [[nodiscard]] virtual RHIBackend backend() const = 0;
    [[nodiscard]] virtual const RHICapabilities& capabilities() const = 0;

    virtual RHIBufferPtr create_buffer(const RHIBufferDesc& desc) = 0;
    virtual RHITexturePtr create_texture(const RHITextureDesc& desc) = 0;
    virtual RHISamplerPtr create_sampler(const RHISamplerDesc& desc) = 0;
    virtual RHIVertexShaderPtr create_vertex_shader(const RHIShaderDesc& desc) = 0;
    virtual RHIPixelShaderPtr create_pixel_shader(const RHIShaderDesc& desc) = 0;
    virtual RHIGraphicsPipelinePtr create_graphics_pipeline(const RHIGraphicsPipelineDesc& desc) = 0;
    virtual RHIRenderTargetPtr create_render_target(const RHIRenderTargetDesc& desc) = 0;
    virtual RHIViewportPtr create_viewport(const RHIViewportDesc& desc) = 0;
    // Copies into a buffer of either memory kind (GpuOnly goes through a staging copy); throws Error when the range exceeds the buffer. Blocks until the copy completes.
    virtual void upload_buffer(RHIBuffer& buffer, uint32_t offset, const uint8_t* data, uint32_t data_size) = 0;
    // Size is in pixels; zero is allowed (the viewport then has no back buffer). The next acquire_back_buffer returns a buffer of the new size; frames in flight keep theirs.
    virtual void resize_viewport(RHIViewport& viewport, uint32_t width, uint32_t height, float scale) = 0;

    // Consumes the list: its retained resources move to the current frame and stay alive until that frame completes.
    virtual void submit(RHICommandList& commands) = 0;
    // Encodes and commits presentation of the viewport's back buffer; it does not end the frame. A non-null `source` is copied into
    // the back buffer first (see rhi_validate_present_source); overrides repeat the default argument.
    virtual void present(RHIViewport& viewport, RHITexture* source = nullptr) = 0;
    // Frame boundary, called once per frame after present (or alone when nothing is presented): advances the frame ring and serial.
    virtual void end_frame() = 0;
    // The ring slot of the frame being recorded, in [0, capabilities().frames_in_flight).
    [[nodiscard]] virtual uint32_t frame_slot() const = 0;
    // Blocking; tests and screenshots only. `out_size` must match the texture's byte size.
    virtual void read_texture(RHITexture& texture, uint8_t* out, uint32_t out_size) = 0;
    virtual void wait_idle() = 0;
};

[[nodiscard]] UniquePtr<IRHI> create_rhi(RHIBackend backend);

} // namespace oryx
