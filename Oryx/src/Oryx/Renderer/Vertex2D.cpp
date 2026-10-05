#include "oxpch.h"
#include "Oryx/Renderer/Vertex2D.h"

namespace oryx
{

template<>
const RHIVertexDeclaration& vertex_declaration<Vertex2DLine>()
{
    static const RHIVertexDeclaration declaration = RHIVertexDeclarationBuilder()
                                                        .stream(0, sizeof(Vertex2DLine))
                                                        .attribute(0, RHIVertexFormat::Float3, offsetof(Vertex2DBase, position))
                                                        .attribute(1, RHIVertexFormat::Float4, offsetof(Vertex2DBase, colour))
                                                        .build();
    return declaration;
}

template<>
const RHIVertexDeclaration& vertex_declaration<Vertex2DQuad>()
{
    static const RHIVertexDeclaration declaration = RHIVertexDeclarationBuilder()
                                                        .stream(0, sizeof(Vertex2DQuad))
                                                        .attribute(0, RHIVertexFormat::Float3, offsetof(Vertex2DBase, position))
                                                        .attribute(1, RHIVertexFormat::Float4, offsetof(Vertex2DBase, colour))
                                                        .attribute(2, RHIVertexFormat::Float2, offsetof(Vertex2DQuad, uv))
                                                        .attribute(3, RHIVertexFormat::Float, offsetof(Vertex2DQuad, tex_index))
                                                        .build();
    return declaration;
}

template<>
const RHIVertexDeclaration& vertex_declaration<Vertex2DCircle>()
{
    static const RHIVertexDeclaration declaration = RHIVertexDeclarationBuilder()
                                                        .stream(0, sizeof(Vertex2DCircle))
                                                        .attribute(0, RHIVertexFormat::Float3, offsetof(Vertex2DBase, position))
                                                        .attribute(1, RHIVertexFormat::Float4, offsetof(Vertex2DBase, colour))
                                                        .attribute(2, RHIVertexFormat::Float2, offsetof(Vertex2DCircle, local_position))
                                                        .attribute(3, RHIVertexFormat::Float, offsetof(Vertex2DCircle, thickness))
                                                        .attribute(4, RHIVertexFormat::Float, offsetof(Vertex2DCircle, fade))
                                                        .build();
    return declaration;
}

template<>
const RHIVertexDeclaration& vertex_declaration<Vertex2DText>()
{
    static const RHIVertexDeclaration declaration = RHIVertexDeclarationBuilder()
                                                        .stream(0, sizeof(Vertex2DText))
                                                        .attribute(0, RHIVertexFormat::Float3, offsetof(Vertex2DBase, position))
                                                        .attribute(1, RHIVertexFormat::Float4, offsetof(Vertex2DBase, colour))
                                                        .attribute(2, RHIVertexFormat::Float2, offsetof(Vertex2DText, uv))
                                                        .attribute(3, RHIVertexFormat::Float, offsetof(Vertex2DText, tex_index))
                                                        .attribute(4, RHIVertexFormat::Float, offsetof(Vertex2DText, px_range))
                                                        .build();
    return declaration;
}

} // namespace oryx
