#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx
{

// pixels may be null (an uninitialised texture); otherwise pixel_bytes must equal width * height * the format's pixel size.
struct Texture2DDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    RHIFormat format = RHIFormat::RGBA8Unorm;
    const uint8_t* pixels = nullptr;
    uint32_t pixel_bytes = 0;
    RHIFilter filter = RHIFilter::Linear;
    RHIAddressMode address = RHIAddressMode::Clamp;
};

class Texture2D
{
public:
    Texture2D(RHITexturePtr texture, RHISamplerPtr sampler);

    static Texture2D create(IRHI& rhi, const Texture2DDesc& desc);

    [[nodiscard]] RHITexture& rhi() const { return *m_texture; }
    [[nodiscard]] const RHITexturePtr& texture() const { return m_texture; }
    [[nodiscard]] const RHISamplerPtr& sampler() const { return m_sampler; }
    [[nodiscard]] uint32_t width() const { return m_texture->width(); }
    [[nodiscard]] uint32_t height() const { return m_texture->height(); }

private:
    RHITexturePtr m_texture;
    RHISamplerPtr m_sampler;
};

} // namespace oryx
