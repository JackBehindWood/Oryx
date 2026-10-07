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

// Glyph quads; px_range is the signed-distance range in atlas pixels, 0 for a coverage atlas.
struct Vertex2DText
{
    Vertex2DBase base;
    Vec2f uv;
    float tex_index;
    float px_range;
};

// How the GUI pixel shader reads a Vertex2DUi; the numbers are shared with Ui.slang.
enum class UiMode : uint8_t
{
    Texture,
    Coverage,
    Distance,
    RoundedFill,
    RoundedBorder
};

// A shape quad is grown by this many world units so its anti-aliased edge is not cut off; Ui.slang subtracts the same amount (OX_UI_SHAPE_MARGIN).
inline constexpr float UI_SHAPE_MARGIN = 1.0f;

// The one vertex of the GUI path: rects, lines, glyphs, images and rounded shapes share a pipeline so they never break a batch.
// control = { texture slot, UiMode, border thickness, px_range }; thickness and radii are fractions of the shape's shorter half-extent. For a shape, uv is the offset from its centre.
struct Vertex2DUi
{
    Vec2f position;
    Vec2f uv;
    uint8_t colour[4];
    uint8_t control[4];
    uint8_t radii[4];
};

static_assert(std::is_standard_layout_v<Vertex2DBase> && std::is_trivially_copyable_v<Vertex2DBase>, "vertex structs are copied to the GPU byte for byte");
static_assert(sizeof(Vertex2DBase) == 28 && offsetof(Vertex2DBase, position) == 0 && offsetof(Vertex2DBase, colour) == 12);
static_assert(sizeof(Vertex2DLine) == 28 && offsetof(Vertex2DLine, base) == 0);
static_assert(sizeof(Vertex2DQuad) == 40 && offsetof(Vertex2DQuad, uv) == 28 && offsetof(Vertex2DQuad, tex_index) == 36);
static_assert(sizeof(Vertex2DText) == 44 && offsetof(Vertex2DText, uv) == 28 && offsetof(Vertex2DText, tex_index) == 36 && offsetof(Vertex2DText, px_range) == 40);
static_assert(sizeof(Vertex2DCircle) == 44 && offsetof(Vertex2DCircle, local_position) == 28 && offsetof(Vertex2DCircle, thickness) == 36 && offsetof(Vertex2DCircle, fade) == 40);

template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DLine>();
template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DQuad>();
template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DCircle>();
static_assert(std::is_standard_layout_v<Vertex2DUi> && std::is_trivially_copyable_v<Vertex2DUi>);
static_assert(sizeof(Vertex2DUi) == 28 && offsetof(Vertex2DUi, uv) == 8 && offsetof(Vertex2DUi, colour) == 16 && offsetof(Vertex2DUi, control) == 20 && offsetof(Vertex2DUi, radii) == 24);

template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DText>();
template<>
[[nodiscard]] const RHIVertexDeclaration& vertex_declaration<Vertex2DUi>();

} // namespace oryx
