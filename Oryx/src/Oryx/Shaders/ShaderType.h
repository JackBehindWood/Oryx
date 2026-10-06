#pragma once

#include "Oryx/Shaders/StaticShader.h"
#include "Oryx/Shaders/PixelShader.h"
#include "Oryx/Shaders/VertexShader.h"

namespace oryx
{

class IRHI;

inline constexpr uint32_t SHADER_MAX_PERMUTATIONS = 64;

// Describes one registered shader class: its source, entry point and how to build an instance from compiler output.
struct ShaderType
{
    const char* name = "";
    const char* source = "";
    const char* entry_point = "main";
    ShaderStage stage = ShaderStage::Vertex;
    std::type_index type = typeid(void);
    ShaderPtr (*create)(IRHI&, const ShaderCompilerOutput&, uint32_t) = nullptr;
    std::vector<ShaderDefine> (*defines_for)(uint32_t) = nullptr;
    bool (*should_compile)(uint32_t) = nullptr;
};

// Every registered type, in registration order; filled by static registrars, never a central list.
[[nodiscard]] const std::vector<ShaderType>& registered_shader_types();

// The defines every shader is compiled with (OX_MAX_TEXTURES), overridden by the type's own for `permutation`.
[[nodiscard]] std::vector<ShaderDefine> shader_defines(const ShaderType& type, uint32_t permutation);

struct ShaderTypeRegistrar
{
    explicit ShaderTypeRegistrar(const ShaderType& type);
};

} // namespace oryx

#define OX_REGISTER_SHADER(Class, Source, Entry, Stage)                                                                                                      \
    namespace                                                                                                                                                \
    {                                                                                                                                                        \
    ::oryx::ShaderPtr create_##Class(::oryx::IRHI& rhi, const ::oryx::ShaderCompilerOutput& output, uint32_t permutation)                          \
    {                                                                                                                                                        \
        return ::oryx::make_ref<Class>(output, permutation, Class::create_rhi_shader(rhi, output, Entry));                                                    \
    }                                                                                                                                                        \
    const ::oryx::ShaderTypeRegistrar registrar_##Class({ #Class, Source, Entry, Stage, typeid(Class), &create_##Class, &Class::defines_for, &Class::should_compile }); \
    }
