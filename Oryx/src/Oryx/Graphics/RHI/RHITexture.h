#pragma once

#include "Oryx/Graphics/RHI/RHIFlags.h"
#include "Oryx/Graphics/RHI/RHIFormat.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

enum class RHITextureUsage : uint8_t
{
    Sampled = BIT(0),
    RenderTarget = BIT(1)
};

template<>
inline constexpr bool rhi_flags_enum<RHITextureUsage> = true;

// initial_data is only read during creation.
struct RHITextureDesc
{
    uint32_t width = 0;
    uint32_t height = 0;
    RHIFormat format = RHIFormat::RGBA8Unorm;
    RHITextureUsage usage = RHITextureUsage::Sampled;
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

protected:
    explicit RHITexture(const RHITextureDesc& desc)
        : RHIResource()
        , m_width(desc.width)
        , m_height(desc.height)
        , m_format(desc.format)
        , m_usage(desc.usage)
    {
    }

private:
    uint32_t m_width;
    uint32_t m_height;
    RHIFormat m_format;
    RHITextureUsage m_usage;
};

using RHITexturePtr = Ref<RHITexture>;

} // namespace oryx
