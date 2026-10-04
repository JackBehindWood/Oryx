#pragma once

#include "Oryx/Shaders/ShaderCompiler.h"

namespace oryx
{

struct ShaderCacheStats
{
    uint32_t hits = 0;
    uint32_t misses = 0;
    uint32_t entries = 0;
};

// In-memory only; references returned stay valid until clear().
class ShaderCache
{
public:
    [[nodiscard]] const ShaderCompilerOutput* find(ShaderHash hash) const;
    // Compiles with the compiler for input.source.language on a miss; a failed compile throws and is not cached.
    const ShaderCompilerOutput& get_or_compile(const ShaderCompilerInput& input);
    void clear();
    [[nodiscard]] ShaderCacheStats stats() const;

private:
    std::unordered_map<ShaderHash, ShaderCompilerOutput> m_entries;
    uint32_t m_hits = 0;
    uint32_t m_misses = 0;
};

} // namespace oryx
