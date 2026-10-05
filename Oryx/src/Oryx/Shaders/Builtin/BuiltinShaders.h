#pragma once

#include "Oryx/Shaders/BuiltinShader.h"
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
    static constexpr uint32_t MAX_TEXTURES = RHI_MAX_TEXTURE_BINDINGS;
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

// Vertex layout: float3 position, float4 colour, float2 uv, float tex_index, float px_range (44 bytes); samples the atlas `.r` as coverage, or as a signed distance when px_range > 0.
class TextVS : public BuiltinShader<VertexShader>
{
public:
    using BuiltinShader::BuiltinShader;
};

// Same texture permutations as QuadPS.
class TextPS : public BuiltinShader<PixelShader>
{
public:
    using BuiltinShader::BuiltinShader;

    static std::vector<ShaderDefine> defines_for(uint32_t permutation);
    static bool should_compile(uint32_t permutation) { return QuadPS::should_compile(permutation); }
};

struct SolidTrianglesEffect : BuiltinEffect<SolidVS, SolidPS>
{
    static constexpr RHITopology TOPOLOGY = RHITopology::Triangles;
    static constexpr bool BLEND = false;
};

struct LineEffect : BuiltinEffect<SolidVS, SolidPS>
{
    static constexpr RHITopology TOPOLOGY = RHITopology::Lines;
    static constexpr bool BLEND = false;
};

struct QuadEffect : BuiltinEffect<QuadVS, QuadPS>
{
    static constexpr RHITopology TOPOLOGY = RHITopology::Triangles;
    static constexpr bool BLEND = true;
};

struct CircleEffect : BuiltinEffect<CircleVS, CirclePS>
{
    static constexpr RHITopology TOPOLOGY = RHITopology::Triangles;
    static constexpr bool BLEND = true;
};

struct TextEffect : BuiltinEffect<TextVS, TextPS>
{
    static constexpr RHITopology TOPOLOGY = RHITopology::Triangles;
    static constexpr bool BLEND = true;
};

} // namespace oryx
