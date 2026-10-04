#pragma once

#include "Oryx/Graphics/Resources/VertexLayout.h"
#include "Oryx/Shaders/ShaderType.h"

namespace oryx
{

// Every built-in shares one frame constants binding: `constant Frame& frame [[buffer(0)]]`, a single float4x4 `view_projection` (64 bytes).
inline constexpr const char* SHADER_FRAME_BINDING = "frame";
inline constexpr uint32_t SHADER_FRAME_SIZE = 64;
// The quad pixel shader's texture array and sampler.
inline constexpr const char* SHADER_TEXTURES_BINDING = "textures";
inline constexpr const char* SHADER_SAMPLER_BINDING = "smp";

// Vertex layout: float3 position, float4 colour.
class SolidVS : public StaticShader<VertexShader>
{
public:
    using StaticShader::StaticShader;
};

class SolidPS : public StaticShader<PixelShader>
{
public:
    using StaticShader::StaticShader;
};

// Vertex layout: float3 position, float4 colour, float2 uv, float tex_index (40 bytes).
class QuadVS : public StaticShader<VertexShader>
{
public:
    using StaticShader::StaticShader;
};

// Permutation 0 samples from 16 textures, permutation 1 from 32.
class QuadPS : public StaticShader<PixelShader>
{
public:
    using StaticShader::StaticShader;

    static constexpr uint32_t DEFAULT_TEXTURES = 16;
    static constexpr uint32_t MAX_TEXTURES = 32;
    [[nodiscard]] static uint32_t texture_count(uint32_t permutation) { return permutation == 0 ? DEFAULT_TEXTURES : MAX_TEXTURES; }
    static std::vector<ShaderDefine> defines_for(uint32_t permutation);
    static bool should_compile(uint32_t permutation) { return permutation < 2; }
};

// Vertex layout: float3 position, float4 colour, float2 local_position, float thickness, float fade (44 bytes).
class CircleVS : public StaticShader<VertexShader>
{
public:
    using StaticShader::StaticShader;
};

class CirclePS : public StaticShader<PixelShader>
{
public:
    using StaticShader::StaticShader;
};

[[nodiscard]] VertexLayout solid_vertex_layout();
[[nodiscard]] VertexLayout quad_vertex_layout();
[[nodiscard]] VertexLayout circle_vertex_layout();

} // namespace oryx
