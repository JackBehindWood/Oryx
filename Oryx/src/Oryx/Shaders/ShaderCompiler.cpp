#include "oxpch.h"
#include "Oryx/Shaders/ShaderCompiler.h"

#include "Oryx/Shaders/MslShaderCompiler.h"

namespace oryx
{

IShaderCompiler& shader_compiler_for(ShaderLanguage language)
{
    static_assert(SHADER_LANGUAGE_COUNT == 1, "add a compiler for the new language");
    static MslShaderCompiler msl;
    switch (language)
    {
    case ShaderLanguage::MSL: return msl;
    }
    return msl;
}

} // namespace oryx
