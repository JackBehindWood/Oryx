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

void validate_texture_shape(const RHITextureDesc& desc)
{
    if (desc.mip_levels != 1)
    {
        throw Error("RHI texture mip_levels must be 1 until mip chains are supported");
    }
    if (desc.array_layers == 0 || desc.sample_count == 0)
    {
        throw Error("RHI texture array layers and sample count must be non-zero");
    }
    const bool multisample = desc.dimension == RHITextureDimension::Tex2DMultisample;
    if (multisample != (desc.sample_count > 1))
    {
        throw Error("RHI texture sample count above one requires the multisample dimension");
    }
    switch (desc.dimension)
    {
    case RHITextureDimension::Tex2D:
        if (desc.array_layers != 1)
        {
            throw Error("RHI 2D texture must have one array layer");
        }
        break;
    case RHITextureDimension::Tex2DArray:
        break;
    case RHITextureDimension::Cube:
        if (desc.width != desc.height || desc.array_layers != 6)
        {
            throw Error("RHI cube texture must be square with six layers");
        }
        break;
    case RHITextureDimension::Tex3D:
        throw Error("RHI 3D textures are not supported by NullRHI");
    case RHITextureDimension::Tex2DMultisample:
        if (desc.array_layers != 1 || desc.initial_data_size != 0 || (desc.sample_count & (desc.sample_count - 1)) != 0 || desc.sample_count > 8)
        {
            throw Error("RHI multisample texture must be single-layer, power-of-two samples up to 8, without initial data");
        }
        break;
    }
    if (has_flag(desc.usage, RHITextureUsage::DepthStencil) && !rhi_format_is_depth(desc.format))
    {
        throw Error("RHI depth-stencil texture needs a depth format");
    }
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

void fill_depth(NullTexture& texture, float depth)
{
    std::vector<uint8_t>& bytes = texture.bytes();
    for (size_t i = 0; i + sizeof(float) <= bytes.size(); i += sizeof(float))
    {
        std::memcpy(bytes.data() + i, &depth, sizeof(float));
    }
}

template<typename T, typename U>
T& require_null(U& resource)
{
    T* converted = dynamic_cast<T*>(&resource);
    if (converted == nullptr)
    {
        throw Error("RHI submit received a resource from a different backend");
    }
    return *converted;
}

// Defers clears until the whole list has executed; the list itself retains what it references.
class NullCommandContext final : public IRHICommandContext
{
public:
    explicit NullCommandContext(NullStats& stats)
        : m_stats(stats)
    {
    }

    void begin_pass(const RHIRenderPassDesc& pass) override
    {
        for (uint32_t i = 0; i < pass.colour_count; ++i)
        {
            NullRenderTarget& target = require_null<NullRenderTarget>(*pass.colour[i].target);
            if (pass.colour[i].load == RHILoadAction::Clear)
            {
                m_clears.emplace_back(&target, pass.colour[i].clear_colour);
            }
        }
        if (pass.depth.texture != nullptr)
        {
            NullTexture& depth = require_null<NullTexture>(*pass.depth.texture);
            if (pass.depth.load == RHILoadAction::Clear)
            {
                m_depth_clears.emplace_back(&depth, pass.depth.clear_depth);
            }
        }
        ++m_stats.passes;
    }

    void set_pipeline(RHIGraphicsPipeline& pipeline) override { require_null<NullPipeline>(pipeline); }
    void set_viewport(const RHIViewportState& viewport) override { m_stats.last_viewport = viewport; }
    void set_scissor(const RHIScissorRect& scissor) override { m_stats.last_scissor = scissor; }
    void set_vertex_buffer(uint32_t, RHIBuffer& buffer, uint32_t) override { require_null<NullBuffer>(buffer); }
    void set_index_buffer(RHIBuffer& buffer, uint32_t, bool) override { require_null<NullBuffer>(buffer); }

    void set_constants(RHIBindingId binding, const uint8_t* data, uint32_t size) override
    {
        m_stats.last_constants_binding = binding;
        m_stats.last_constants.assign(data, data + size);
    }

    void bind_buffer(RHIBindingId, RHIBuffer& buffer, uint32_t, uint32_t) override { require_null<NullBuffer>(buffer); }
    void bind_texture(RHIBindingId, RHITexture& texture, uint32_t) override { require_null<NullTexture>(texture); }
    void bind_sampler(RHIBindingId, RHISampler& sampler, uint32_t) override { require_null<NullSampler>(sampler); }
    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override { ++m_stats.draw_calls; }
    void draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override { ++m_stats.indexed_draw_calls; }
    void push_debug_group(const char* name) override { m_stats.debug_events.push_back(std::string("push:") + name); }
    void pop_debug_group() override { m_stats.debug_events.push_back("pop"); }
    void end_pass() override {}

    void copy_buffer(RHIBuffer& source, uint32_t source_offset, RHIBuffer& destination, uint32_t destination_offset, uint32_t size) override
    {
        const NullBuffer& from = require_null<NullBuffer>(source);
        require_null<NullBuffer>(destination).write(destination_offset, from.bytes().data() + source_offset, size);
    }

    void apply_clears() const
    {
        for (const std::pair<NullRenderTarget*, Colour>& clear : m_clears)
        {
            fill_colour(clear.first->texture(), clear.second);
        }
        for (const std::pair<NullTexture*, float>& clear : m_depth_clears)
        {
            fill_depth(*clear.first, clear.second);
        }
    }

private:
    NullStats& m_stats;
    std::vector<std::pair<NullRenderTarget*, Colour>> m_clears;
    std::vector<std::pair<NullTexture*, float>> m_depth_clears;
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
    write(offset, data, data_size);
}

uint8_t* NullBuffer::map()
{
    if (memory() != RHIMemory::CpuToGpu)
    {
        throw Error("RHI buffer map requires CpuToGpu memory");
    }
    return m_bytes.data();
}

void NullBuffer::write(uint32_t offset, const uint8_t* data, uint32_t data_size)
{
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
    , m_bytes(static_cast<size_t>(desc.width) * desc.height * desc.array_layers * rhi_format_bytes(desc.format))
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

void NullViewport::resize(uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;
}

RHIRenderTargetPtr NullViewport::acquire_back_buffer()
{
    if (m_back_buffer && (m_back_buffer->width() != m_width || m_back_buffer->height() != m_height))
    {
        m_back_buffer.reset();
    }
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
    m_capabilities.max_texture_bindings = RHI_MAX_TEXTURE_BINDINGS;
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
    validate_texture_shape(desc);
    return make_ref<NullTexture>(desc);
}

RHISamplerPtr NullRHI::create_sampler(const RHISamplerDesc& desc)
{
    return make_ref<NullSampler>(desc);
}

RHIVertexShaderPtr NullRHI::create_vertex_shader(const RHIShaderDesc& desc)
{
    if (desc.stage != RHIShaderStage::Vertex)
    {
        throw Error("create_vertex_shader requires RHIShaderStage::Vertex");
    }
    return make_ref<NullVertexShader>(desc);
}

RHIPixelShaderPtr NullRHI::create_pixel_shader(const RHIShaderDesc& desc)
{
    if (desc.stage != RHIShaderStage::Pixel)
    {
        throw Error("create_pixel_shader requires RHIShaderStage::Pixel");
    }
    return make_ref<NullPixelShader>(desc);
}

RHIGraphicsPipelinePtr NullRHI::create_graphics_pipeline(const RHIGraphicsPipelineDesc& desc)
{
    rhi_validate_graphics_pipeline_desc(desc);
    for (uint32_t i = 0; i < desc.colour_format_count; ++i)
    {
        if (!rhi_format_is_colour(desc.colour_formats[i]))
        {
            throw Error("RHI pipeline colour format is not supported by NullRHI");
        }
    }
    for (uint32_t i = 0; i < desc.binding_count; ++i)
    {
        if (desc.bindings[i].kind == RHIBindingKind::StorageBuffer || desc.bindings[i].kind == RHIBindingKind::StorageTexture)
        {
            throw Error("RHI storage bindings are not supported by NullRHI");
        }
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
    if (desc.colour->dimension() != RHITextureDimension::Tex2D || desc.colour->sample_count() != 1)
    {
        throw Error("RHI render target texture must be a single-sample 2D texture");
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

void NullRHI::resize_viewport(RHIViewport* viewport, uint32_t width, uint32_t height, float)
{
    rhi_require_non_null(viewport, "resize_viewport");
    NullViewport* null_viewport = dynamic_cast<NullViewport*>(viewport);
    if (null_viewport == nullptr)
    {
        throw Error("RHI resize_viewport received a viewport from a different backend");
    }
    null_viewport->resize(width, height);
}

void NullRHI::submit(RHICommandList& commands)
{
    if (commands.in_pass())
    {
        throw Error("RHI submit received a command list with an unfinished pass");
    }

    if (commands.debug_depth() != 0)
    {
        throw Error("RHI submit received a command list with an unbalanced debug group");
    }

    NullCommandContext context(m_stats);
    commands.execute(context);
    context.apply_clears();

    m_last_submission.clear();
    for (const RHICommand& command : commands)
    {
        m_last_submission.push_back(command.command_name());
    }
    ++m_submit_count;
    commands.drain_into(m_frame_slots[m_slot]);
}

void NullRHI::present(RHIViewport* viewport, RHITexture* source)
{
    rhi_require_non_null(viewport, "present");
    if (source != nullptr)
    {
        rhi_validate_present_source(*viewport, *source);
        NullTexture* source_texture = dynamic_cast<NullTexture*>(source);
        RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
        NullRenderTarget* target = dynamic_cast<NullRenderTarget*>(back_buffer.get());
        if (source_texture == nullptr || target == nullptr)
        {
            throw Error("RHI present received a resource from a different backend");
        }
        target->texture().bytes() = source_texture->bytes();
    }
}

void NullRHI::end_frame()
{
    m_frame_slots[m_slot].clear();
    m_slot = (m_slot + 1) % m_frame_slots.size();
    ++m_frame_count;
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

void NullRHI::upload_buffer(RHIBuffer* buffer, uint32_t offset, const uint8_t* data, uint32_t data_size)
{
    rhi_require_non_null(buffer, "upload_buffer");
    NullBuffer* null_buffer = dynamic_cast<NullBuffer*>(buffer);
    if (null_buffer == nullptr)
    {
        throw Error("RHI upload_buffer received a buffer from a different backend");
    }
    null_buffer->write(offset, data, data_size);
}

void NullRHI::read_texture(RHITexture* texture, uint8_t* out, uint32_t out_size)
{
    rhi_require_non_null(texture, "read_texture");
    NullTexture* null_texture = dynamic_cast<NullTexture*>(texture);
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
