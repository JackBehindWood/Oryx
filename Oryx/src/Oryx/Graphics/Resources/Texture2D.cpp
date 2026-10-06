#include "oxpch.h"
#include "Oryx/Graphics/Resources/Texture2D.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

Texture2D::Texture2D(RHITexturePtr texture, RHISamplerPtr sampler)
    : m_texture(std::move(texture))
    , m_sampler(std::move(sampler))
{
    if (!m_texture || !m_sampler)
    {
        throw Error("Texture2D requires an RHI texture and sampler");
    }
}

Texture2D Texture2D::create(IRHI& rhi, const Texture2DDesc& desc)
{
    if (desc.pixels != nullptr && desc.pixel_bytes != static_cast<size_t>(desc.width) * desc.height * rhi_format_bytes(desc.format))
    {
        throw Error("Texture2D pixel data does not match the texture size");
    }
    RHITexturePtr texture = rhi.create_texture({ .width = desc.width, .height = desc.height, .format = desc.format, .usage = RHITextureUsage::Sampled, .initial_data = desc.pixels, .initial_data_size = desc.pixels != nullptr ? desc.pixel_bytes : 0 });
    RHISamplerPtr sampler = rhi.create_sampler({ .min_filter = desc.filter, .mag_filter = desc.filter, .address_u = desc.address, .address_v = desc.address });
    return Texture2D(std::move(texture), std::move(sampler));
}

} // namespace oryx
