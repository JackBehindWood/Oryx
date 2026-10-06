#pragma once

#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

struct SlangCompilerOptions
{
    // Empty falls through to OX_SLANGC_PATH, then PATH; OX_SLANGC in the environment always wins.
    std::filesystem::path slangc;
};

// Runs `slangc -target metal` once per entry point and reads its reflection JSON; the tool is only needed on a cache miss.
class SlangCompiler final : public IShaderCompiler
{
public:
    SlangCompiler() = default;
    explicit SlangCompiler(SlangCompilerOptions options)
        : m_options(std::move(options))
    {
    }

    [[nodiscard]] const char* id() const override { return "oryx-slang"; }
    // Bump with the slangc pin in forge.toml, so stale compiled output is rebuilt.
    [[nodiscard]] uint32_t version() const override { return 1; }
    [[nodiscard]] std::vector<std::string> dependencies(const ShaderSource& source) const override;
    [[nodiscard]] ShaderCompilerOutput compile(const ShaderCompilerInput& input) const override;

private:
    SlangCompilerOptions m_options;
};

// `OX_SLANGC`, else `options.slangc` (shaders.slangc), else the path forge resolved at build time (OX_SLANGC_PATH), else `slangc` on PATH.
[[nodiscard]] std::filesystem::path slangc_path(const SlangCompilerOptions& options = {});

// Translates a slangc reflection JSON document of one entry point; throws Error for constructs a Metal pipeline cannot bind.
[[nodiscard]] ShaderReflection parse_slang_reflection(const std::string& json, const std::string& entry_point, ShaderStage stage);

} // namespace oryx
