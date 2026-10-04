#include "oxpch.h"
#include "Oryx/Graphics/Resources/RenderTarget.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

RenderTarget::RenderTarget(RHIRenderTargetPtr target, Texture2D colour)
    : m_target(std::move(target))
    , m_colour(std::move(colour))
{
    if (!m_target)
    {
        throw Error("RenderTarget requires an RHI render target");
    }
}

RenderTarget RenderTarget::create(IRHI& rhi, uint32_t width, uint32_t height, RHIFormat format)
{
    RHITexturePtr texture = rhi.create_texture({ .width = width, .height = height, .format = format, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    return RenderTarget(std::move(target), Texture2D(std::move(texture), rhi.create_sampler({})));
}

} // namespace oryx
