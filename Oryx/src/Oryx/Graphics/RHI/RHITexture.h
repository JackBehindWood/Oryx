#pragma once

#include "Oryx/Graphics/RHI/RHIFlags.h"
#include "Oryx/Graphics/RHI/RHIFormat.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

enum class RHITextureUsage : uint8_t
{
    Sampled = BIT(0),
    RenderTarget = BIT(1),
    DepthStencil = BIT(2)
};

template<>
inline constexpr bool rhi_flags_enum<RHITextureUsage> = true;

enum class RHITextureDimension : uint8_t
{
    Tex2D,
    Tex2DArray,
    Cube,
    Tex3D,
    Tex2DMultisample
};

// initial_data is only read during creation.
struct RHITextureDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    RHIFormat format = RHIFormat::RGBA8Unorm;
    RHITextureUsage usage = RHITextureUsage::Sampled;
    RHITextureDimension dimension = RHITextureDimension::Tex2D;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    uint32_t sample_count = 1;
    const uint8_t* initial_data = nullptr;
    uint32_t initial_data_size = 0;
    const char* name = nullptr;
};

class RHITexture : public RHIResource
{
public:
    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }
    [[nodiscard]] RHIFormat format() const { return m_format; }
    [[nodiscard]] RHITextureUsage usage() const { return m_usage; }
    [[nodiscard]] RHITextureDimension dimension() const { return m_dimension; }
    [[nodiscard]] uint32_t mip_levels() const { return m_mip_levels; }
    [[nodiscard]] uint32_t array_layers() const { return m_array_layers; }
    [[nodiscard]] uint32_t sample_count() const { return m_sample_count; }

protected:
    explicit RHITexture(const RHITextureDesc& desc)
        : RHIResource()
        , m_width(desc.width)
        , m_height(desc.height)
        , m_format(desc.format)
        , m_usage(desc.usage)
        , m_dimension(desc.dimension)
        , m_mip_levels(desc.mip_levels)
        , m_array_layers(desc.array_layers)
        , m_sample_count(desc.sample_count)
    {
    }

private:
    uint32_t m_width;
    uint32_t m_height;
    RHIFormat m_format;
    RHITextureUsage m_usage;
    RHITextureDimension m_dimension;
    uint32_t m_mip_levels;
    uint32_t m_array_layers;
    uint32_t m_sample_count;
};

using RHITexturePtr = Ref<RHITexture>;

} // namespace oryx
