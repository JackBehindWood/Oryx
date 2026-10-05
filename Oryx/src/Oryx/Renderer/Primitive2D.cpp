#include "oxpch.h"
#include "Oryx/Renderer/Primitive2D.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Renderer/Vertex2D.h"

namespace oryx
{

PrimitiveTraits primitive_traits(Primitive2D primitive)
{
    switch (primitive)
    {
    case Primitive2D::Quad: return { sizeof(Vertex2DQuad), 4, 6, RHITopology::Triangles, BuiltinPipeline::Quad, true, false };
    case Primitive2D::Circle: return { sizeof(Vertex2DCircle), 4, 6, RHITopology::Triangles, BuiltinPipeline::Circle, true, false };
    case Primitive2D::Line: return { sizeof(Vertex2DLine), 2, 0, RHITopology::Lines, BuiltinPipeline::SolidLines, false, false };
    case Primitive2D::Triangle: return { sizeof(Vertex2DLine), 3, 0, RHITopology::Triangles, BuiltinPipeline::SolidTriangles, false, false };
    case Primitive2D::Text: return { sizeof(Vertex2DText), 4, 6, RHITopology::Triangles, BuiltinPipeline::Text, true, false };
    }
    throw Error("Primitive2D is invalid");
}

} // namespace oryx
