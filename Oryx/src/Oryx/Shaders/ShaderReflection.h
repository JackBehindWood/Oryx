#pragma once

#include "Oryx/Shaders/ShaderStage.h"

namespace oryx
{

enum class ShaderScalar : uint8_t
{
    Float,
    Half,
    Int,
    UInt,
    Bool
};

// Scalars and vectors have columns == 1; a matrix is `columns` columns of `rows` scalars.
struct ShaderDataType
{
    ShaderScalar scalar = ShaderScalar::Float;
    uint8_t rows = 1;
    uint8_t columns = 1;

    friend bool operator==(const ShaderDataType&, const ShaderDataType&) = default;
};

[[nodiscard]] uint32_t shader_type_size(const ShaderDataType& type);
[[nodiscard]] uint32_t shader_type_alignment(const ShaderDataType& type);
[[nodiscard]] std::string shader_type_name(const ShaderDataType& type);
// Parses MSL spellings such as `float`, `half3`, `uint2`, `float4x4`; returns false for anything else.
[[nodiscard]] bool parse_shader_type(std::string_view name, ShaderDataType& out);

enum class ShaderBindingKind : uint8_t
{
    Constants,
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler
};

enum class ShaderTextureData : uint8_t
{
    Float,
    Int,
    UInt,
    Depth
};

enum class ShaderTextureDimension : uint8_t
{
    Tex2D,
    Tex2DArray,
    Cube,
    Tex3D,
    Tex2DMultisample
};

struct ShaderStructMember
{
    std::string name;
    ShaderDataType type;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct ShaderStageVariable
{
    std::string name;
    uint32_t location = 0;
    ShaderDataType type;
    bool builtin = false;
};

// size is the struct byte size for Constants and 0 (unbounded) for pointer buffers; the texture fields apply to textures.
struct ShaderBinding
{
    std::string name;
    ShaderBindingKind kind = ShaderBindingKind::Constants;
    uint32_t slot = 0;
    uint32_t array_count = 1;
    uint32_t size = 0;
    ShaderTextureData texture_data_type = ShaderTextureData::Float;
    ShaderTextureDimension texture_dimension = ShaderTextureDimension::Tex2D;
    std::vector<ShaderStructMember> members;
};

using ShaderParameterMap = std::vector<ShaderBinding>;

[[nodiscard]] const ShaderBinding* find_binding(const ShaderParameterMap& parameters, std::string_view name);

struct ShaderReflection
{
    std::string entry_point;
    ShaderStage stage = ShaderStage::Vertex;
    std::vector<ShaderStageVariable> inputs;
    std::vector<ShaderStageVariable> outputs;
    ShaderParameterMap parameters;
    // Zero until compute shaders exist.
    uint32_t thread_group_size[3] = { 0, 0, 0 };
};

} // namespace oryx
