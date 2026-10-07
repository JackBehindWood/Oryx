#include "oxpch.h"
#include "Oryx/Renderer/Batch/Primitive2D.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Renderer/Batch/Vertex2D.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

PrimitiveTraits primitive_traits(Primitive2D primitive)
{
    switch (primitive)
    {
    case Primitive2D::Quad: return { sizeof(Vertex2DQuad), 4, 6, RHITopology::Triangles, true, true, false };
    case Primitive2D::Circle: return { sizeof(Vertex2DCircle), 4, 6, RHITopology::Triangles, false, true, false };
    case Primitive2D::Line: return { sizeof(Vertex2DLine), 2, 0, RHITopology::Lines, false, false, false };
    case Primitive2D::Triangle: return { sizeof(Vertex2DLine), 3, 0, RHITopology::Triangles, false, false, false };
    case Primitive2D::Text: return { sizeof(Vertex2DText), 4, 6, RHITopology::Triangles, true, true, false };
    }
    throw Error("Primitive2D is invalid");
}

const char* primitive_name(Primitive2D primitive)
{
    switch (primitive)
    {
    case Primitive2D::Quad: return "quad";
    case Primitive2D::Circle: return "circle";
    case Primitive2D::Line: return "line";
    case Primitive2D::Triangle: return "triangle";
    case Primitive2D::Text: return "text";
    }
    throw Error("Primitive2D is invalid");
}

const PipelineDef& pipeline_def(Primitive2D primitive)
{
    static const PipelineDef defs[PRIMITIVE_2D_COUNT] = {
        make_pipeline_def<QuadVS, QuadPS, Vertex2DQuad>(RHITopology::Triangles, true),
        make_pipeline_def<CircleVS, CirclePS, Vertex2DCircle>(RHITopology::Triangles, true),
        make_pipeline_def<SolidVS, SolidPS, Vertex2DLine>(RHITopology::Lines, false),
        make_pipeline_def<SolidVS, SolidPS, Vertex2DLine>(RHITopology::Triangles, false),
        make_pipeline_def<TextVS, TextPS, Vertex2DText>(RHITopology::Triangles, true),
    };
    const uint32_t index = static_cast<uint32_t>(primitive);
    if (index >= PRIMITIVE_2D_COUNT)
    {
        throw Error("Primitive2D is invalid");
    }
    return defs[index];
}

} // namespace oryx
