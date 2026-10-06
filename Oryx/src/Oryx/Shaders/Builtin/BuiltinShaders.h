#pragma once

#include "Oryx/Shaders/ShaderType.h"
#include "Oryx/Shaders/StaticShader.h"

namespace oryx
{

struct TextureArrayDimension
{
    static constexpr const char* NAME = "OX_MAX_TEXTURES";
    static constexpr uint32_t DEFAULT_TEXTURES = 16;
    static constexpr uint32_t MAX_TEXTURES = RHI_MAX_TEXTURE_BINDINGS;
    static constexpr uint32_t VALUES[] = { DEFAULT_TEXTURES, MAX_TEXTURES };
};

// Permutation 0 samples from 16 textures, permutation 1 from 32; the pixel shaders with a `textures` array use it.
struct TextureArrayPermutations : ShaderPermutationDomain<TextureArrayDimension>
{
    static constexpr uint32_t DEFAULT_TEXTURES = TextureArrayDimension::DEFAULT_TEXTURES;
    static constexpr uint32_t MAX_TEXTURES = TextureArrayDimension::MAX_TEXTURES;
};

// Marks a shader the engine owns (registered from Shaders/Builtin); StaticShader stays the generic base for application shaders.
template<typename StageShader, typename Domain = SinglePermutation>
class BuiltinShader : public StaticShader<StageShader, Domain>
{
public:
    using StaticShader<StageShader, Domain>::StaticShader;
};

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

class QuadPS : public BuiltinShader<PixelShader, TextureArrayPermutations>
{
public:
    using BuiltinShader::BuiltinShader;
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

class TextPS : public BuiltinShader<PixelShader, TextureArrayPermutations>
{
public:
    using BuiltinShader::BuiltinShader;
};

} // namespace oryx
