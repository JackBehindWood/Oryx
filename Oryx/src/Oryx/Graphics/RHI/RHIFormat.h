#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Core/Base.h"

namespace oryx
{

[[nodiscard]] constexpr size_t rhi_format_bytes(RHIFormat format)
{
    switch (format)
    {
    case RHIFormat::Undefined: return 0;
    case RHIFormat::R8Unorm: return 1;
    case RHIFormat::RGBA8Unorm: return 4;
    case RHIFormat::BGRA8Unorm: return 4;
    case RHIFormat::RGBA16Float: return 8;
    case RHIFormat::Depth32Float: return 4;
    }
    return 0;
}

[[nodiscard]] constexpr bool rhi_format_is_depth(RHIFormat format)
{
    return format == RHIFormat::Depth32Float;
}

[[nodiscard]] constexpr RHIDataType rhi_format_data_type(RHIFormat format)
{
    return rhi_format_is_depth(format) ? RHIDataType::Depth : RHIDataType::Float;
}

[[nodiscard]] constexpr bool rhi_format_is_colour(RHIFormat format)
{
    return format == RHIFormat::R8Unorm || format == RHIFormat::RGBA8Unorm || format == RHIFormat::BGRA8Unorm;
}

} // namespace oryx
