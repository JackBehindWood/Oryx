#pragma once

#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Renderer/BuiltinPipelines.h"

namespace oryx
{

enum class Primitive2D : uint8_t
{
    Quad,
    Circle,
    Line,
    Triangle
};

inline constexpr uint32_t PRIMITIVE_2D_COUNT = 4;

// How a primitive is expanded into vertices; `instanced` is the seam for one-vertex-per-instance quads and is false for every primitive today.
struct PrimitiveTraits
{
    uint32_t vertex_size = 0;
    uint32_t vertices_per_primitive = 0;
    uint32_t indices_per_primitive = 0;
    RHITopology topology = RHITopology::Triangles;
    BuiltinPipeline pipeline = BuiltinPipeline::Quad;
    bool indexed = false;
    bool instanced = false;
};

[[nodiscard]] PrimitiveTraits primitive_traits(Primitive2D primitive);

} // namespace oryx
