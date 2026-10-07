#pragma once

#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Renderer/PipelineDef.h"

namespace oryx
{

enum class Primitive2D : uint8_t
{
    Quad,
    Circle,
    Line,
    Triangle,
    Text,
    Ui
};

inline constexpr uint32_t PRIMITIVE_2D_COUNT = 6;

// How a primitive is expanded into vertices; `instanced` is the seam for one-vertex-per-instance quads and is false for every primitive today.
struct PrimitiveTraits
{
    uint32_t vertex_size = 0;
    uint32_t vertices_per_primitive = 0;
    uint32_t indices_per_primitive = 0;
    RHITopology topology = RHITopology::Triangles;
    bool textured = false;
    bool indexed = false;
    bool instanced = false;
};

[[nodiscard]] PrimitiveTraits primitive_traits(Primitive2D primitive);
[[nodiscard]] const char* primitive_name(Primitive2D primitive);
// The pipeline each primitive draws with; built on first use because shader types register during static initialisation.
[[nodiscard]] const PipelineDef& pipeline_def(Primitive2D primitive);

} // namespace oryx
