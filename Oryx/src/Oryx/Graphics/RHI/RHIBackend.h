#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

enum class RHIBackend : uint8_t
{
    Null,
    Metal,
    OpenGL,
    Vulkan,
    D3D12,
    WebGPU
};

[[nodiscard]] std::string to_string(RHIBackend backend);
[[nodiscard]] RHIBackend parse_rhi_backend(std::string_view str);
[[nodiscard]] RHIBackend default_rhi_backend();

} // namespace oryx
