#pragma once

#include "MetalApi.h"
#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIRenderTarget.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx::metal
{

class MetalBuffer final : public RHIBuffer
{
public:
    MetalBuffer(const RHIBufferDesc& desc, NS::SharedPtr<MTL::Buffer> buffer);

    void update(uint32_t offset, const uint8_t* data, uint32_t data_size) override;

    [[nodiscard]] MTL::Buffer* mtl() const { return m_buffer.get(); }

private:
    NS::SharedPtr<MTL::Buffer> m_buffer;
};

class MetalTexture final : public RHITexture
{
public:
    MetalTexture(const RHITextureDesc& desc, NS::SharedPtr<MTL::Texture> texture)
        : RHITexture(desc)
        , m_texture(std::move(texture))
    {
    }

    [[nodiscard]] MTL::Texture* mtl() const { return m_texture.get(); }

private:
    NS::SharedPtr<MTL::Texture> m_texture;
};

class MetalSampler final : public RHISampler
{
public:
    MetalSampler(const RHISamplerDesc& desc, NS::SharedPtr<MTL::SamplerState> sampler)
        : RHISampler(desc)
        , m_sampler(std::move(sampler))
    {
    }

    [[nodiscard]] MTL::SamplerState* mtl() const { return m_sampler.get(); }

private:
    NS::SharedPtr<MTL::SamplerState> m_sampler;
};

// Offscreen targets expose their colour texture; viewport back buffers (offscreen or drawable-backed) do not.
class MetalRenderTarget final : public RHIRenderTarget
{
public:
    MetalRenderTarget(NS::SharedPtr<MTL::Texture> texture, uint32_t width, uint32_t height, RHIFormat format, Ref<MetalTexture> colour, NS::SharedPtr<CA::MetalDrawable> drawable)
        : m_texture(std::move(texture))
        , m_colour(std::move(colour))
        , m_drawable(std::move(drawable))
        , m_width(width)
        , m_height(height)
        , m_format(format)
    {
    }

    [[nodiscard]] uint32_t width() const override { return m_width; }
    [[nodiscard]] uint32_t height() const override { return m_height; }
    [[nodiscard]] RHIFormat format() const override { return m_format; }
    [[nodiscard]] RHITexturePtr colour() const override { return RHITexturePtr(m_colour); }

    [[nodiscard]] MTL::Texture* mtl() const { return m_texture.get(); }
    [[nodiscard]] CA::MetalDrawable* drawable() const { return m_drawable.get(); }

private:
    NS::SharedPtr<MTL::Texture> m_texture;
    Ref<MetalTexture> m_colour;
    NS::SharedPtr<CA::MetalDrawable> m_drawable;
    uint32_t m_width;
    uint32_t m_height;
    RHIFormat m_format;
};

} // namespace oryx::metal
