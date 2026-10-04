#include "oxpch.h"
#include "MetalRHI.h"

#include "MetalCommandContext.h"
#include "MetalCommandQueue.h"
#include "MetalConvert.h"
#include "MetalDevice.h"
#include "MetalPipeline.h"
#include "MetalResources.h"
#include "MetalViewport.h"
#include "Oryx/Core/Error.h"

#include <pthread.h>

namespace oryx
{

using namespace metal;

MetalRHI::MetalRHI()
    : m_queue(m_device, m_lease)
{
}

MetalRHI::~MetalRHI()
{
    if (pthread_main_np() == 0)
    {
        OX_CORE_ERROR("MetalRHI destroyed off the main thread");
    }
    try
    {
        m_queue.wait_idle();
    }
    catch (const Error& error)
    {
        OX_CORE_ERROR("{}", error.what());
    }
}

const RHICapabilities& MetalRHI::capabilities() const
{
    return m_device.capabilities();
}

RHIBufferPtr MetalRHI::create_buffer(const RHIBufferDesc& desc)
{
    OX_METAL_AUTORELEASE_SCOPE;
    return make_ref<MetalBuffer>(desc, m_device.make_buffer(desc));
}

RHITexturePtr MetalRHI::create_texture(const RHITextureDesc& desc)
{
    if (desc.format == RHIFormat::Undefined)
    {
        throw Error("RHI texture format is undefined");
    }
    OX_METAL_AUTORELEASE_SCOPE;
    return make_ref<MetalTexture>(desc, m_device.make_texture(desc));
}

RHISamplerPtr MetalRHI::create_sampler(const RHISamplerDesc& desc)
{
    return make_ref<MetalSampler>(desc, m_device.make_sampler(desc));
}

namespace
{

NS::SharedPtr<MTL::Function> make_function(MetalDevice& device, const RHIShaderDesc& desc, RHIShaderStage expected, const char* what)
{
    if (desc.stage != expected)
    {
        throw Error(std::string(what) + " requires the matching RHIShaderStage");
    }
    if (desc.entry_point == nullptr)
    {
        throw Error("RHI shader has no entry point");
    }
    NS::SharedPtr<MTL::Library> library = device.make_library(desc.code, desc.code_size);
    NS::SharedPtr<MTL::Function> function = NS::TransferPtr(library->newFunction(NS::String::string(desc.entry_point, NS::UTF8StringEncoding)));
    if (!function)
    {
        throw Error(std::string("Metal shader has no entry point named '") + desc.entry_point + "'");
    }
    return function;
}

} // namespace

RHIVertexShaderPtr MetalRHI::create_vertex_shader(const RHIShaderDesc& desc)
{
    OX_METAL_AUTORELEASE_SCOPE;
    return make_ref<MetalVertexShader>(desc, make_function(m_device, desc, RHIShaderStage::Vertex, "create_vertex_shader"));
}

RHIPixelShaderPtr MetalRHI::create_pixel_shader(const RHIShaderDesc& desc)
{
    OX_METAL_AUTORELEASE_SCOPE;
    return make_ref<MetalPixelShader>(desc, make_function(m_device, desc, RHIShaderStage::Pixel, "create_pixel_shader"));
}

RHIGraphicsPipelinePtr MetalRHI::create_graphics_pipeline(const RHIGraphicsPipelineDesc& desc)
{
    OX_METAL_AUTORELEASE_SCOPE;
    return create_metal_pipeline(m_device, desc);
}

RHIRenderTargetPtr MetalRHI::create_render_target(const RHIRenderTargetDesc& desc)
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
        throw Error("RHI render target format is not a colour format");
    }
    MetalTexture* texture = dynamic_cast<MetalTexture*>(desc.colour.get());
    if (texture == nullptr)
    {
        throw Error("RHI render target texture belongs to a different backend");
    }
    return make_ref<MetalRenderTarget>(NS::RetainPtr(texture->mtl()), texture->width(), texture->height(), texture->format(), Ref<MetalTexture>::from_raw(texture), NS::SharedPtr<CA::MetalDrawable>());
}

RHIViewportPtr MetalRHI::create_viewport(const RHIViewportDesc& desc)
{
    return make_ref<MetalViewport>(m_device, desc);
}

void MetalRHI::resize_viewport(RHIViewport& viewport, uint32_t width, uint32_t height, float scale)
{
    MetalViewport* metal_viewport = dynamic_cast<MetalViewport*>(&viewport);
    if (metal_viewport == nullptr)
    {
        throw Error("RHI resize_viewport received a viewport from a different backend");
    }
    OX_METAL_AUTORELEASE_SCOPE;
    metal_viewport->resize(width, height, scale);
}

void MetalRHI::submit(RHICommandList& commands)
{
    if (commands.in_pass())
    {
        throw Error("RHI submit received a command list with an unfinished pass");
    }
    if (commands.debug_depth() != 0)
    {
        throw Error("RHI submit received a command list with an unbalanced debug group");
    }
    if (commands.empty())
    {
        return;
    }

    OX_METAL_AUTORELEASE_SCOPE;
    m_queue.poll();
    NS::SharedPtr<MTL::CommandBuffer> encoded = m_queue.make_command_buffer();
    {
        MetalCommandContext context(*encoded.get());
        commands.execute(context);
    }
    m_queue.commit(std::move(encoded));
    commands.drain_into(m_queue.frame_refs());
}

void MetalRHI::present(RHIViewport& viewport, RHITexture* source)
{
    MetalViewport* metal_viewport = dynamic_cast<MetalViewport*>(&viewport);
    if (metal_viewport == nullptr)
    {
        throw Error("RHI present received a viewport from a different backend");
    }

    OX_METAL_AUTORELEASE_SCOPE;
    NS::SharedPtr<MTL::CommandBuffer> commands;
    if (source != nullptr)
    {
        rhi_validate_present_source(viewport, *source);
        RHIRenderTargetPtr back_buffer = viewport.acquire_back_buffer();
        if (back_buffer)
        {
            MetalTexture* source_texture = dynamic_cast<MetalTexture*>(source);
            MetalRenderTarget* target = dynamic_cast<MetalRenderTarget*>(back_buffer.get());
            if (source_texture == nullptr || target == nullptr)
            {
                throw Error("RHI present received a resource from a different backend");
            }
            commands = m_queue.make_command_buffer();
            MTL::BlitCommandEncoder* blit = commands->blitCommandEncoder();
            blit->copyFromTexture(source_texture->mtl(), target->mtl());
            blit->endEncoding();
            m_queue.frame_refs().push_back(Ref<RHIResource>::from_raw(source));
            m_queue.frame_refs().push_back(Ref<RHIResource>(back_buffer));
        }
    }
    if (metal_viewport->has_drawable())
    {
        if (!commands)
        {
            commands = m_queue.make_command_buffer();
        }
        metal_viewport->encode_present(*commands.get());
    }
    if (commands)
    {
        m_queue.commit(std::move(commands));
    }
}

void MetalRHI::end_frame()
{
    OX_METAL_AUTORELEASE_SCOPE;
    m_queue.end_frame();
}

void MetalRHI::read_texture(RHITexture& texture, uint8_t* out, uint32_t out_size)
{
    MetalTexture* metal_texture = dynamic_cast<MetalTexture*>(&texture);
    if (metal_texture == nullptr)
    {
        throw Error("RHI read_texture received a texture from a different backend");
    }
    const uint32_t bytes_per_row = texture.width() * static_cast<uint32_t>(rhi_format_bytes(texture.format()));
    const uint32_t byte_count = bytes_per_row * texture.height();
    if (out_size != byte_count)
    {
        throw Error("RHI read_texture output size does not match the texture size");
    }

    OX_METAL_AUTORELEASE_SCOPE;
    NS::SharedPtr<MTL::Buffer> staging = require_object(NS::TransferPtr(m_device.device()->newBuffer(byte_count, MTL::ResourceStorageModeShared)), "a readback buffer");
    NS::SharedPtr<MTL::CommandBuffer> commands = m_queue.make_command_buffer();
    MTL::BlitCommandEncoder* blit = commands->blitCommandEncoder();
    const MTL::BlitOption options = texture.format() == RHIFormat::Depth32Float ? MTL::BlitOptionDepthFromDepthStencil : MTL::BlitOptionNone;
    blit->copyFromTexture(metal_texture->mtl(), 0, 0, MTL::Origin(0, 0, 0), MTL::Size(texture.width(), texture.height(), 1), staging.get(), 0, bytes_per_row, byte_count, options);
    blit->endEncoding();
    commands->commit();
    commands->waitUntilCompleted();
    std::string failure = command_buffer_failure(*commands.get());
    if (!failure.empty())
    {
        throw Error("Metal texture readback failed", failure);
    }
    std::memcpy(out, staging->contents(), byte_count);
}

void MetalRHI::wait_idle()
{
    OX_METAL_AUTORELEASE_SCOPE;
    m_queue.wait_idle();
}

} // namespace oryx
