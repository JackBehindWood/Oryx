#pragma once

#include "Oryx/Graphics/RHI/RHIFlags.h"
#include "Oryx/Graphics/RHI/RHIFormat.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx
{

// An index into the current pipeline's binding table.
using RHIBindingId = uint32_t;

inline constexpr uint32_t RHI_MAX_BINDINGS = 32;
inline constexpr RHIBindingId RHI_INVALID_BINDING = std::numeric_limits<RHIBindingId>::max();
inline constexpr uint32_t RHI_MAX_CONSTANTS_SIZE = 4096;

enum class RHIBindingKind : uint8_t
{
    Constants,
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler
};

enum class RHIShaderStageMask : uint8_t
{
    Vertex = BIT(0),
    Pixel = BIT(1)
};

template<>
inline constexpr bool rhi_flags_enum<RHIShaderStageMask> = true;

// The layout a pipeline is created with: slot mapping and validation data only, never names or reflection.
// `size` is the byte size for Constants and the maximum bound range for buffers (0 = unbounded); the texture fields apply to textures.
struct RHIBindingDesc
{
    RHIBindingKind kind = RHIBindingKind::Constants;
    RHIShaderStageMask stage_mask = RHIShaderStageMask::Vertex;
    RHIDataType data_type = RHIDataType::Float;
    RHITextureDimension texture_dimension = RHITextureDimension::Tex2D;
    uint32_t slot = 0;
    uint32_t array_count = 1;
    uint32_t size = 0;
};

[[nodiscard]] constexpr bool rhi_binding_is_buffer(RHIBindingKind kind)
{
    return kind == RHIBindingKind::UniformBuffer || kind == RHIBindingKind::StorageBuffer;
}

[[nodiscard]] constexpr bool rhi_binding_is_texture(RHIBindingKind kind)
{
    return kind == RHIBindingKind::SampledTexture || kind == RHIBindingKind::StorageTexture;
}

} // namespace oryx
