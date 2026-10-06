#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

class NullBuffer final : public RHIBuffer
{
public:
    explicit NullBuffer(const RHIBufferDesc& desc);

    void update(uint32_t offset, const uint8_t* data, uint32_t data_size) override;
    [[nodiscard]] uint8_t* map() override;
    void write(uint32_t offset, const uint8_t* data, uint32_t data_size);

    [[nodiscard]] const std::vector<uint8_t>& bytes() const { return m_bytes; }

private:
    std::vector<uint8_t> m_bytes;
};

class NullTexture final : public RHITexture
{
public:
    explicit NullTexture(const RHITextureDesc& desc);

    [[nodiscard]] const std::vector<uint8_t>& bytes() const { return m_bytes; }
    [[nodiscard]] std::vector<uint8_t>& bytes() { return m_bytes; }

private:
    std::vector<uint8_t> m_bytes;
};

class NullSampler final : public RHISampler
{
public:
    explicit NullSampler(const RHISamplerDesc& desc)
        : RHISampler(desc)
    {
    }
};

class NullVertexShader final : public RHIVertexShader
{
public:
    explicit NullVertexShader(const RHIShaderDesc& desc)
        : RHIVertexShader(desc)
    {
    }
};

class NullPixelShader final : public RHIPixelShader
{
public:
    explicit NullPixelShader(const RHIShaderDesc& desc)
        : RHIPixelShader(desc)
    {
    }
};

class NullPipeline final : public RHIGraphicsPipeline
{
public:
    explicit NullPipeline(const RHIGraphicsPipelineDesc& desc)
        : RHIGraphicsPipeline(desc)
    {
    }
};

class NullRenderTarget final : public RHIRenderTarget
{
public:
    NullRenderTarget(Ref<NullTexture> texture, bool offscreen)
        : m_texture(std::move(texture))
        , m_offscreen(offscreen)
    {
    }

    [[nodiscard]] uint32_t width() const override { return m_texture->width(); }
    [[nodiscard]] uint32_t height() const override { return m_texture->height(); }
    [[nodiscard]] RHIFormat format() const override { return m_texture->format(); }
    [[nodiscard]] RHITexturePtr colour() const override { return m_offscreen ? RHITexturePtr(m_texture) : RHITexturePtr(); }

    [[nodiscard]] NullTexture& texture() const { return *m_texture; }

private:
    Ref<NullTexture> m_texture;
    bool m_offscreen;
};

class NullViewport final : public RHIViewport
{
public:
    explicit NullViewport(const RHIViewportDesc& desc);

    void resize(uint32_t width, uint32_t height);
    void set_vsync(bool vsync) { m_vsync = vsync; }
    [[nodiscard]] bool vsync() const { return m_vsync; }
    [[nodiscard]] uint32_t width() const override { return m_width; }
    [[nodiscard]] uint32_t height() const override { return m_height; }
    [[nodiscard]] RHIFormat format() const override { return m_format; }
    [[nodiscard]] RHIRenderTargetPtr acquire_back_buffer() override;
    void discard_back_buffer() override { m_back_buffer.reset(); }

private:
    RHIFormat m_format;
    uint32_t m_width;
    uint32_t m_height;
    bool m_vsync;
    Ref<NullRenderTarget> m_back_buffer;
};

// What the context saw across all submissions; the test-visible record of draws, state and debug groups.
struct NullStats
{
    uint32_t draw_calls = 0;
    uint32_t indexed_draw_calls = 0;
    uint32_t passes = 0;
    RHIBindingId last_constants_binding = RHI_INVALID_BINDING;
    std::vector<uint8_t> last_constants;
    RHIViewportState last_viewport;
    RHIScissorRect last_scissor;
    std::vector<std::string> debug_events;
};

struct NullRHIOptions
{
    // A frame's resources stay alive until its ring slot is re-entered, as with a GPU `frames_in_flight` deep; off retires them at end_frame.
    bool simulate_latency = true;
};

// Headless device: real CPU-side storage and clears, no GPU. Test-visible counters expose what was submitted.
class NullRHI final : public IRHI
{
public:
    explicit NullRHI(NullRHIOptions options = {});
    ~NullRHI() override;

    [[nodiscard]] RHIBackend backend() const override { return RHIBackend::Null; }
    [[nodiscard]] const RHICapabilities& capabilities() const override { return m_capabilities; }

    RHIBufferPtr create_buffer(const RHIBufferDesc& desc) override;
    RHITexturePtr create_texture(const RHITextureDesc& desc) override;
    RHISamplerPtr create_sampler(const RHISamplerDesc& desc) override;
    RHIVertexShaderPtr create_vertex_shader(const RHIShaderDesc& desc) override;
    RHIPixelShaderPtr create_pixel_shader(const RHIShaderDesc& desc) override;
    RHIGraphicsPipelinePtr create_graphics_pipeline(const RHIGraphicsPipelineDesc& desc) override;
    RHIRenderTargetPtr create_render_target(const RHIRenderTargetDesc& desc) override;
    RHIViewportPtr create_viewport(const RHIViewportDesc& desc) override;
    void resize_viewport(RHIViewport* viewport, uint32_t width, uint32_t height, float scale) override;
    void set_viewport_vsync(RHIViewport* viewport, bool vsync) override;

    void submit(RHICommandList& commands) override;
    void present(RHIViewport* viewport, RHITexture* source = nullptr) override;
    void end_frame() override;
    [[nodiscard]] uint32_t frame_slot() const override { return static_cast<uint32_t>(m_slot); }
    void upload_buffer(RHIBuffer* buffer, uint32_t offset, const uint8_t* data, uint32_t data_size) override;
    void read_texture(RHITexture* texture, uint8_t* out, uint32_t out_size) override;
    void wait_idle() override;

    [[nodiscard]] size_t live_resources() const { return RHIResource::live_count(); }
    [[nodiscard]] size_t submit_count() const { return m_submit_count; }
    [[nodiscard]] const std::vector<std::string_view>& last_submission() const { return m_last_submission; }
    [[nodiscard]] size_t frame_count() const { return m_frame_count; }
    [[nodiscard]] const NullStats& stats() const { return m_stats; }
    // The next end_frame throws as a lost device or failed command buffer would, without advancing the frame ring.
    void fail_next_end_frame() { m_fail_next_end_frame = true; }

private:
    // Declared first so it is destroyed last: it drains retired resources once the device is gone.
    RHIDeviceLease m_lease;
    RHICapabilities m_capabilities;
    // One reusable slot per frame in flight; the slot's capacity survives present.
    std::vector<std::vector<Ref<RHIResource>>> m_frame_slots;
    std::vector<uint64_t> m_slot_serials;
    NullRHIOptions m_options;
    bool m_fail_next_end_frame = false;
    size_t m_slot = 0;
    std::vector<std::string_view> m_last_submission;
    size_t m_submit_count = 0;
    size_t m_frame_count = 0;
    NullStats m_stats;
};

} // namespace oryx
