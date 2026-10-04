#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// Compute and tessellation stages are reserved; no compiler or backend handles them yet.
enum class ShaderStage : uint8_t
{
    Vertex,
    Pixel,
    Compute,
    TessControl,
    TessEval
};

inline constexpr uint32_t SHADER_STAGE_COUNT = static_cast<uint32_t>(ShaderStage::TessEval) + 1;

[[nodiscard]] const char* shader_stage_name(ShaderStage stage);

} // namespace oryx
