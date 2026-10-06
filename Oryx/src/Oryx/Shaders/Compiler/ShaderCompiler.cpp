#include "oxpch.h"
#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

#include "Oryx/Shaders/Compiler/MslShaderCompiler.h"
#include "Oryx/Shaders/Compiler/SlangCompiler.h"

namespace oryx
{

IShaderCompiler& shader_compiler_for(ShaderLanguage language)
{
    static_assert(SHADER_LANGUAGE_COUNT == 2, "add a compiler for the new language");
    static MslShaderCompiler msl;
    static SlangCompiler slang;
    switch (language)
    {
    case ShaderLanguage::MSL: return msl;
    case ShaderLanguage::Slang: return slang;
    }
    return msl;
}

} // namespace oryx
