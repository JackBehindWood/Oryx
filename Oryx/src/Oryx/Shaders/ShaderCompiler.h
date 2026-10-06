#pragma once

#include "Oryx/Shaders/ShaderReflection.h"
#include "Oryx/Shaders/ShaderSource.h"

namespace oryx
{

using ShaderHash = uint64_t;

struct ShaderDefine
{
    std::string name;
    std::string value;
};

enum class ShaderBinaryFormat : uint8_t
{
    MslSource
};

struct ShaderCompilerInput
{
    ShaderSource source;
    ShaderStage stage = ShaderStage::Vertex;
    std::string entry_point = "main";
    std::vector<ShaderDefine> defines;
    // Resolved include texts by name, ahead of the process-wide registry; filled by ShaderLibrary from its source provider.
    std::map<std::string, std::string> includes;
};

struct ShaderCompilerOutput
{
    std::vector<uint8_t> binary;
    ShaderBinaryFormat format = ShaderBinaryFormat::MslSource;
    ShaderReflection reflection;
    ShaderHash hash = 0;
    std::vector<std::string> diagnostics;
};

class IShaderCompiler
{
public:
    virtual ~IShaderCompiler() = default;

    [[nodiscard]] virtual const char* id() const = 0;
    [[nodiscard]] virtual uint32_t version() const = 0;
    // Throws Error with a `source:line: message` text on any diagnostic error.
    [[nodiscard]] virtual ShaderCompilerOutput compile(const ShaderCompilerInput& input) const = 0;
};

[[nodiscard]] IShaderCompiler& shader_compiler_for(ShaderLanguage language);

} // namespace oryx
