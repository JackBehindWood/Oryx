#pragma once

#include "Oryx/Graphics/Resources/VertexLayout.h"
#include "Oryx/Shaders/ShaderType.h"

namespace oryx
{

// Every built-in shares one frame constants binding: `constant Frame& frame [[buffer(0)]]`, a single float4x4 `view_projection` (64 bytes).
inline constexpr const char* BUILTIN_FRAME_BINDING = "frame";
inline constexpr uint32_t BUILTIN_FRAME_SIZE = 64;

// Vertex layout: float3 position, float4 colour.
class SolidVS : public BuiltinShader<VertexShader>
{
public:
    using BuiltinShader::BuiltinShader;
};

class SolidPS : public BuiltinShader<PixelShader>
{
public:
    using BuiltinShader::BuiltinShader;
};

// Vertex layout: float3 position, float4 colour, float2 uv, float tex_index (40 bytes).
class QuadVS : public BuiltinShader<VertexShader>
{
public:
    using BuiltinShader::BuiltinShader;
};

// Permutation 0 samples from 16 textures, permutation 1 from 32.
class QuadPS : public BuiltinShader<PixelShader>
{
public:
    using BuiltinShader::BuiltinShader;

    static constexpr uint32_t DEFAULT_TEXTURES = 16;
    static constexpr uint32_t MAX_TEXTURES = 32;
    [[nodiscard]] static uint32_t texture_count(uint32_t permutation) { return permutation == 0 ? DEFAULT_TEXTURES : MAX_TEXTURES; }
    static std::vector<ShaderDefine> defines_for(uint32_t permutation);
    static bool should_compile(uint32_t permutation) { return permutation < 2; }
};

// Vertex layout: float3 position, float4 colour, float2 local_position, float thickness, float fade (44 bytes).
class CircleVS : public BuiltinShader<VertexShader>
{
public:
    using BuiltinShader::BuiltinShader;
};

class CirclePS : public BuiltinShader<PixelShader>
{
public:
    using BuiltinShader::BuiltinShader;
};

[[nodiscard]] VertexLayout solid_vertex_layout();
[[nodiscard]] VertexLayout quad_vertex_layout();
[[nodiscard]] VertexLayout circle_vertex_layout();

} // namespace oryx
