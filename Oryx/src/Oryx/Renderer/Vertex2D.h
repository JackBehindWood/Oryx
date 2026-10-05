#pragma once

#include "Oryx/Graphics/RHI/RHIVertexDeclaration.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Math/Vector3.h"
#include "Oryx/Math/Vector4.h"

namespace oryx
{

// CPU mirrors of the built-in 2D vertex formats; the matching declarations come from vertex_declaration<T>().
struct Vertex2DBase
{
    Vec3f position;
    Vec4f colour;
};

// Lines and solid triangles.
struct Vertex2DLine
{
    Vertex2DBase base;
};

struct Vertex2DQuad
{
    Vertex2DBase base;
    Vec2f uv;
    float tex_index;
};

struct Vertex2DCircle
{
    Vertex2DBase base;
    Vec2f local_position;
    float thickness;
    float fade;
};

static_assert(std::is_standard_layout_v<Vertex2DBase> && std::is_trivially_copyable_v<Vertex2DBase>, "vertex structs are copied to the GPU byte for byte");
static_assert(sizeof(Vertex2DBase) == 28 && offsetof(Vertex2DBase, position) == 0 && offsetof(Vertex2DBase, colour) == 12);
static_assert(sizeof(Vertex2DLine) == 28 && offsetof(Vertex2DLine, base) == 0);
static_assert(sizeof(Vertex2DQuad) == 40 && offsetof(Vertex2DQuad, uv) == 28 && offsetof(Vertex2DQuad, tex_index) == 36);
static_assert(sizeof(Vertex2DCircle) == 44 && offsetof(Vertex2DCircle, local_position) == 28 && offsetof(Vertex2DCircle, thickness) == 36 && offsetof(Vertex2DCircle, fade) == 40);

// Specialised for each Vertex2D* type; the declaration is built once.
template<typename T>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration();

template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DLine>();
template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DQuad>();
template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DCircle>();

} // namespace oryx
