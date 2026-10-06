#pragma once

#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

// Headless parser for a restricted MSL subset; it reflects one entry point and validates the interface, it does not type-check function bodies.
class MslShaderCompiler final : public IShaderCompiler
{
public:
    [[nodiscard]] const char* id() const override { return "oryx-msl"; }
    [[nodiscard]] uint32_t version() const override { return 1; }
    [[nodiscard]] std::vector<std::string> dependencies(const ShaderSource& source) const override;
    [[nodiscard]] ShaderCompilerOutput compile(const ShaderCompilerInput& input) const override;
};

} // namespace oryx
