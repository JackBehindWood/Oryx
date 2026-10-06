#pragma once

#include "Oryx/Shaders/Cache/ShaderCache.h"
#include "Oryx/Shaders/Source/ShaderSourceProvider.h"

namespace oryx
{

struct ShaderCookResult
{
    uint32_t shaders = 0;
    uint64_t map_id = 0;
};

// Compiles every registered shader type and permutation from `sources` through `cache`, writes each binary to `store`, then the map for
// the registered type set. Needs no RHI; throws Error naming the shader that failed.
ShaderCookResult cook_shaders(ShaderCache& cache, const IShaderSourceProvider& sources, const IShaderBinaryStore& store);

} // namespace oryx
