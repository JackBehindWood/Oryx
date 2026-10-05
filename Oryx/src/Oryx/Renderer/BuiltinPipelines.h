#pragma once

#include "Oryx/Renderer/GraphicsPipelineCache.h"
#include "Oryx/Shaders/ShaderLibrary.h"

namespace oryx
{

class IRHI;

// The engine's fixed pipelines; each maps to one effect in Shaders/Builtin/BuiltinShaders.h.
enum class BuiltinPipeline : uint8_t
{
    SolidTriangles,
    SolidLines,
    Quad,
    Circle
};

inline constexpr uint32_t BUILTIN_PIPELINE_COUNT = 4;
// QuadPS is the only effect with permutations (see QuadPS::should_compile).
inline constexpr uint32_t BUILTIN_MAX_PERMUTATIONS = 2;

// Throws Error for a permutation the pipeline's pixel shader does not compile.
[[nodiscard]] GraphicsPipelineDesc builtin_pipeline_desc(BuiltinPipeline pipeline, const ShaderLibrary& shaders, RHIFormat colour_format, uint32_t permutation = 0);

// Lazily builds the engine's pipelines through a GraphicsPipelineCache and remembers their handles until the cache is cleared.
class BuiltinPipelines
{
public:
    [[nodiscard]] GraphicsPipelineHandle get(IRHI& rhi, GraphicsPipelineCache& cache, const ShaderLibrary& shaders, RHIFormat colour_format, BuiltinPipeline pipeline, uint32_t permutation = 0);
    void reset();

private:
    GraphicsPipelineHandle m_handles[BUILTIN_PIPELINE_COUNT][BUILTIN_MAX_PERMUTATIONS];
    uint32_t m_generation = 0;
    RHIFormat m_format = RHIFormat::Undefined;
};

} // namespace oryx
