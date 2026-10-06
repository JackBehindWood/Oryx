#pragma once

#include "Oryx/Shaders/ShaderReflection.h"
#include "Oryx/Shaders/Source/ShaderSource.h"

namespace oryx
{

using ShaderHash = uint64_t;

struct ShaderDefine
{
    std::string name;
    std::string value;
};

// Persisted in compiled shaders: values are explicit and only ever appended.
enum class ShaderBinaryFormat : uint8_t
{
    MslSource = 0,
    MetalLib = 1
};

struct ShaderCompilerInput
{
    ShaderSource source;
    ShaderStage stage = ShaderStage::Vertex;
    std::string entry_point = "main";
    std::vector<ShaderDefine> defines;
    // What the compiler should emit; part of the cache key so one source built for two targets never collides.
    ShaderBinaryFormat target = ShaderBinaryFormat::MslSource;
    // Resolved include texts by name, ahead of the process-wide registry; filled by ShaderLibrary from its source provider.
    std::map<std::string, std::string> includes;
};

struct ShaderCompilerOutput
{
    std::vector<uint8_t> binary;
    ShaderBinaryFormat format = ShaderBinaryFormat::MslSource;
    ShaderReflection reflection;
    ShaderHash hash = 0;
    // Which compiler produced this; filled by ShaderCache and persisted with the output.
    ShaderLanguage language = ShaderLanguage::MSL;
    std::string compiler_id;
    uint32_t compiler_version = 0;
    std::vector<std::string> diagnostics;
};

class IShaderCompiler
{
public:
    virtual ~IShaderCompiler() = default;

    [[nodiscard]] virtual const char* id() const = 0;
    [[nodiscard]] virtual uint32_t version() const = 0;
    // The files `source` includes or imports directly, as names that map to virtual paths through shader_include_virtual_path;
    // callers follow them transitively. Must not shell out or touch the filesystem.
    [[nodiscard]] virtual std::vector<std::string> dependencies(const ShaderSource& source) const = 0;
    // Throws Error with a `source:line: message` text on any diagnostic error.
    [[nodiscard]] virtual ShaderCompilerOutput compile(const ShaderCompilerInput& input) const = 0;
};

[[nodiscard]] IShaderCompiler& shader_compiler_for(ShaderLanguage language);

} // namespace oryx
