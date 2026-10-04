#include "oxpch.h"
#include "MetalViewport.h"

#include "MetalConvert.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

namespace
{

constexpr uint32_t DRAWABLE_COUNT = 3;

bool layer_supports(RHIFormat format)
{
    return format == RHIFormat::BGRA8Unorm || format == RHIFormat::RGBA8Unorm || format == RHIFormat::RGBA16Float;
}

} // namespace

MetalViewport::MetalViewport(const MetalDevice& device, const RHIViewportDesc& desc)
    : m_device(device)
    , m_format(desc.format)
    , m_width(desc.width)
    , m_height(desc.height)
{
    if (!rhi_format_is_colour(desc.format))
    {
        throw Error("RHI viewport format is not a colour format");
    }
    if (desc.native_window == nullptr)
    {
        return;
    }
    if (!layer_supports(desc.format))
    {
        throw Error("RHI viewport format is not supported by a Metal layer");
    }

    m_surface = create_unique<MetalSurface>(desc.native_window, device.device(), MetalSurfaceDesc{ desc.vsync });
    CA::MetalLayer* layer = m_surface->layer();
    layer->setPixelFormat(to_mtl(desc.format));
    layer->setFramebufferOnly(false);
    layer->setMaximumDrawableCount(DRAWABLE_COUNT);
    layer->setAllowsNextDrawableTimeout(true);
    m_surface->set_colourspace_srgb();
    if (desc.width > 0 && desc.height > 0)
    {
        m_surface->resize(desc.width, desc.height, desc.scale);
    }
}

uint32_t MetalViewport::width() const
{
    return m_surface ? m_surface->width_px() : m_width;
}

uint32_t MetalViewport::height() const
{
    return m_surface ? m_surface->height_px() : m_height;
}

void MetalViewport::resize(uint32_t width, uint32_t height, float scale)
{
    if (m_surface)
    {
        m_surface->resize(width, height, scale);
        return;
    }
    m_width = width;
    m_height = height;
}

RHIRenderTargetPtr MetalViewport::acquire_back_buffer()
{
    const uint32_t target_width = width();
    const uint32_t target_height = height();
    if (m_back_buffer && (m_back_buffer->width() != target_width || m_back_buffer->height() != target_height))
    {
        m_back_buffer.reset();
    }
    if (m_back_buffer)
    {
        return m_back_buffer;
    }
    if (target_width == 0 || target_height == 0)
    {
        return {};
    }

    OX_METAL_AUTORELEASE_SCOPE;
    if (m_surface)
    {
        if (!m_surface->is_visible())
        {
            return {};
        }
        NS::SharedPtr<CA::MetalDrawable> drawable = NS::RetainPtr(m_surface->layer()->nextDrawable());
        if (!drawable)
        {
            return {};
        }
        NS::SharedPtr<MTL::Texture> texture = NS::RetainPtr(drawable->texture());
        m_back_buffer = make_ref<MetalRenderTarget>(std::move(texture), target_width, target_height, m_format, Ref<MetalTexture>(), std::move(drawable));
        return m_back_buffer;
    }

    RHITextureDesc texture_desc;
    texture_desc.width = target_width;
    texture_desc.height = target_height;
    texture_desc.format = m_format;
    texture_desc.usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled;
    NS::SharedPtr<MTL::Texture> texture = m_device.make_texture(texture_desc);
    m_back_buffer = make_ref<MetalRenderTarget>(std::move(texture), target_width, target_height, m_format, Ref<MetalTexture>(), NS::SharedPtr<CA::MetalDrawable>());
    return m_back_buffer;
}

void MetalViewport::discard_back_buffer()
{
    m_back_buffer.reset();
}

void MetalViewport::encode_present(MTL::CommandBuffer& commands)
{
    if (has_drawable())
    {
        commands.presentDrawable(m_back_buffer->drawable());
        m_back_buffer.reset();
    }
}

} // namespace oryx::metal
