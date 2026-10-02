#include "oxpch.h"
#include "NullRHI.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

constexpr uint32_t NULL_MAX_TEXTURE_SIZE = 16384;
constexpr uint32_t NULL_FRAMES_IN_FLIGHT = 3;

uint8_t to_unorm8(float value)
{
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

void fill_colour(NullTexture& texture, const Colour& colour)
{
    std::vector<uint8_t>& bytes = texture.bytes();
    switch (texture.format())
    {
    case RHIFormat::R8Unorm:
        std::fill(bytes.begin(), bytes.end(), to_unorm8(colour.r));
        return;
    case RHIFormat::RGBA8Unorm:
    case RHIFormat::BGRA8Unorm:
    {
        const bool bgra = texture.format() == RHIFormat::BGRA8Unorm;
        const std::array<uint8_t, 4> pixel = {
            to_unorm8(bgra ? colour.b : colour.r), to_unorm8(colour.g), to_unorm8(bgra ? colour.r : colour.b), to_unorm8(colour.a)
        };
        for (size_t i = 0; i + pixel.size() <= bytes.size(); i += pixel.size())
        {
            std::memcpy(bytes.data() + i, pixel.data(), pixel.size());
        }
        return;
    }
    case RHIFormat::Undefined:
    case RHIFormat::RGBA16Float:
    case RHIFormat::Depth32Float:
        break;
    }
    throw Error("NullRHI cannot clear a texture of this format");
}

// Defers clears until the whole list has executed; the list itself retains what it references.
class NullCommandContext final : public IRHICommandContext
{
public:
    void begin_pass(RHIRenderTarget& target, const RHIClear& clear) override
    {
        NullRenderTarget* null_target = dynamic_cast<NullRenderTarget*>(&target);
        if (null_target == nullptr)
        {
            throw Error("RHI submit received a render target from a different backend");
        }
        if (clear.clear)
        {
            m_clears.emplace_back(null_target, clear.colour);
        }
    }

    void set_pipeline(RHIGraphicsPipeline&) override {}
    void set_vertex_buffer(uint32_t, RHIBuffer&, uint32_t) override {}
    void set_index_buffer(RHIBuffer&, uint32_t, bool) override {}
    void bind_buffer(RHIBindingId, RHIBuffer&) override {}
    void bind_texture(RHIBindingId, RHITexture&) override {}
    void bind_sampler(RHIBindingId, RHISampler&) override {}
    void draw(uint32_t, uint32_t, uint32_t) override {}
    void draw_indexed(uint32_t, uint32_t, uint32_t) override {}
    void end_pass() override {}

    void apply_clears() const
    {
        for (const std::pair<NullRenderTarget*, Colour>& clear : m_clears)
        {
            fill_colour(clear.first->texture(), clear.second);
        }
    }

private:
    std::vector<std::pair<NullRenderTarget*, Colour>> m_clears;
};

} // namespace

NullBuffer::NullBuffer(const RHIBufferDesc& desc)
    : RHIBuffer(desc)
    , m_bytes(desc.size)
{
    if (desc.initial_data_size > desc.size)
    {
        throw Error("RHI buffer initial data exceeds the buffer size");
    }
    if (desc.initial_data_size > 0)
    {
        std::memcpy(m_bytes.data(), desc.initial_data, desc.initial_data_size);
    }
}

void NullBuffer::update(uint32_t offset, const uint8_t* data, uint32_t data_size)
{
    if (memory() != RHIMemory::CpuToGpu)
    {
        throw Error("RHI buffer update requires CpuToGpu memory");
    }
    if (offset > size() || data_size > size() - offset)
    {
        throw Error("RHI buffer update is out of range");
    }
    if (data_size > 0)
    {
        std::memcpy(m_bytes.data() + offset, data, data_size);
    }
}

NullTexture::NullTexture(const RHITextureDesc& desc)
    : RHITexture(desc)
    , m_bytes(static_cast<size_t>(desc.width) * desc.height * rhi_format_bytes(desc.format))
{
    if (desc.initial_data_size != 0 && desc.initial_data_size != m_bytes.size())
    {
        throw Error("RHI texture initial data does not match the texture size");
    }
    if (desc.initial_data_size > 0)
    {
        std::memcpy(m_bytes.data(), desc.initial_data, desc.initial_data_size);
    }
}

NullViewport::NullViewport(const RHIViewportDesc& desc)
    : m_format(desc.format)
    , m_width(desc.width)
    , m_height(desc.height)
{
}

void NullViewport::resize(uint32_t width, uint32_t height, float)
{
    if (width != m_width || height != m_height)
    {
        m_width = width;
        m_height = height;
        m_back_buffer.reset();
    }
}

RHIRenderTargetPtr NullViewport::acquire_back_buffer()
{
    if (m_width == 0 || m_height == 0)
    {
        return {};
    }
    if (!m_back_buffer)
    {
        RHITextureDesc texture_desc;
        texture_desc.width = m_width;
        texture_desc.height = m_height;
        texture_desc.format = m_format;
        texture_desc.usage = RHITextureUsage::RenderTarget;
        Ref<NullTexture> texture = make_ref<NullTexture>(texture_desc);
        m_back_buffer = make_ref<NullRenderTarget>(std::move(texture), false);
    }
    return m_back_buffer;
}

NullRHI::NullRHI()
{
    m_capabilities.name = "Null";
    m_capabilities.max_texture_size = NULL_MAX_TEXTURE_SIZE;
    m_capabilities.frames_in_flight = NULL_FRAMES_IN_FLIGHT;
    m_frame_slots.resize(NULL_FRAMES_IN_FLIGHT);
}

NullRHI::~NullRHI()
{
    wait_idle();
}

RHIBufferPtr NullRHI::create_buffer(const RHIBufferDesc& desc)
{
    if (desc.size == 0)
    {
        throw Error("RHI buffer size must be non-zero");
    }
    return make_ref<NullBuffer>(desc);
}

RHITexturePtr NullRHI::create_texture(const RHITextureDesc& desc)
{
    if (desc.width == 0 || desc.height == 0 || desc.width > NULL_MAX_TEXTURE_SIZE || desc.height > NULL_MAX_TEXTURE_SIZE)
    {
        throw Error("RHI texture dimensions are out of range");
    }
    if (desc.format == RHIFormat::Undefined)
    {
        throw Error("RHI texture format is undefined");
    }
    return make_ref<NullTexture>(desc);
}

RHISamplerPtr NullRHI::create_sampler(const RHISamplerDesc& desc)
{
    return make_ref<NullSampler>(desc);
}

RHIVertexShaderPtr NullRHI::create_vertex_shader(const RHIShaderDesc& desc)
{
    if (desc.stage != ShaderStage::Vertex)
    {
        throw Error("create_vertex_shader requires ShaderStage::Vertex");
    }
    return make_ref<NullVertexShader>(desc);
}

RHIPixelShaderPtr NullRHI::create_pixel_shader(const RHIShaderDesc& desc)
{
    if (desc.stage != ShaderStage::Pixel)
    {
        throw Error("create_pixel_shader requires ShaderStage::Pixel");
    }
    return make_ref<NullPixelShader>(desc);
}

RHIGraphicsPipelinePtr NullRHI::create_graphics_pipeline(const RHIGraphicsPipelineDesc& desc)
{
    if (!desc.vertex || !desc.pixel)
    {
        throw Error("RHI pipeline requires a vertex and a pixel shader");
    }
    if (!rhi_format_is_colour(desc.colour_format))
    {
        throw Error("RHI pipeline colour format is not supported by NullRHI");
    }
    return make_ref<NullPipeline>(desc);
}

RHIRenderTargetPtr NullRHI::create_render_target(const RHIRenderTargetDesc& desc)
{
    if (!desc.colour)
    {
        throw Error("RHI render target requires a colour texture");
    }
    if (!has_flag(desc.colour->usage(), RHITextureUsage::RenderTarget))
    {
        throw Error("RHI render target texture was not created with RHITextureUsage::RenderTarget");
    }
    if (!rhi_format_is_colour(desc.colour->format()))
    {
        throw Error("RHI render target format is not supported by NullRHI");
    }
    NullTexture* texture = dynamic_cast<NullTexture*>(desc.colour.get());
    if (texture == nullptr)
    {
        throw Error("RHI render target texture belongs to a different backend");
    }
    return make_ref<NullRenderTarget>(Ref<NullTexture>::from_raw(texture), true);
}

RHIViewportPtr NullRHI::create_viewport(const RHIViewportDesc& desc)
{
    if (!rhi_format_is_colour(desc.format))
    {
        throw Error("RHI viewport format is not supported by NullRHI");
    }
    return make_ref<NullViewport>(desc);
}

void NullRHI::submit(RHICommandList& commands)
{
    if (commands.in_pass())
    {
        throw Error("RHI submit received a command list with an unfinished pass");
    }

    NullCommandContext context;
    commands.execute(context);
    context.apply_clears();

    m_last_submission.clear();
    for (const RHICommand& command : commands)
    {
        m_last_submission.push_back(command.type());
    }
    ++m_submit_count;
    commands.drain_into(m_frame_slots[m_slot]);
}

void NullRHI::present(RHIViewport& viewport, RHITexture* source)
{
    if (source != nullptr)
    {
        rhi_validate_present_source(viewport, *source);
        NullTexture* source_texture = dynamic_cast<NullTexture*>(source);
        RHIRenderTargetPtr back_buffer = viewport.acquire_back_buffer();
        NullRenderTarget* target = dynamic_cast<NullRenderTarget*>(back_buffer.get());
        if (source_texture == nullptr || target == nullptr)
        {
            throw Error("RHI present received a resource from a different backend");
        }
        target->texture().bytes() = source_texture->bytes();
    }
    m_frame_slots[m_slot].clear();
    m_slot = (m_slot + 1) % m_frame_slots.size();
    m_lease.end_frame();
    m_lease.set_idle();
}

void NullRHI::wait_idle()
{
    for (std::vector<Ref<RHIResource>>& slot : m_frame_slots)
    {
        slot.clear();
    }
    m_lease.end_frame();
    m_lease.set_idle();
}

void NullRHI::read_texture(RHITexture& texture, uint8_t* out, uint32_t out_size)
{
    NullTexture* null_texture = dynamic_cast<NullTexture*>(&texture);
    if (null_texture == nullptr)
    {
        throw Error("RHI read_texture received a texture from a different backend");
    }
    const std::vector<uint8_t>& bytes = null_texture->bytes();
    if (out_size != bytes.size())
    {
        throw Error("RHI read_texture output size does not match the texture size");
    }
    std::memcpy(out, bytes.data(), bytes.size());
}

} // namespace oryx
