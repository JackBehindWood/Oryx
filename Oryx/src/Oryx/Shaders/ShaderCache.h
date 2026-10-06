#pragma once

#include "Oryx/Shaders/ShaderBinaryStore.h"
#include "Oryx/Shaders/ShaderCompiler.h"

namespace oryx
{

struct ShaderCacheStats
{
    uint32_t hits = 0;
    uint32_t misses = 0;
    uint32_t store_hits = 0;
    uint32_t entries = 0;
};

// Memory first, then the optional persistent store, then the compiler; references returned stay valid until clear().
class ShaderCache
{
public:
    // The store is borrowed and must outlive the cache; null (the default) keeps the cache in memory only.
    void set_store(const IShaderBinaryStore* store) { m_store = store; }
    [[nodiscard]] const IShaderBinaryStore* store() const { return m_store; }

    [[nodiscard]] const ShaderCompilerOutput* find(ShaderHash hash) const;
    // Compiles with the compiler for input.source.language on a miss; a failed compile throws and is not cached.
    const ShaderCompilerOutput& get_or_compile(const ShaderCompilerInput& input);
    // Same with an explicit compiler (tests, tools); a store hit never calls it beyond id/version/dependencies.
    const ShaderCompilerOutput& get_or_compile(const ShaderCompilerInput& input, const IShaderCompiler& compiler);
    void clear();
    [[nodiscard]] ShaderCacheStats stats() const;

private:
    std::unordered_map<ShaderHash, ShaderCompilerOutput> m_entries;
    uint32_t m_hits = 0;
    uint32_t m_misses = 0;
    uint32_t m_store_hits = 0;
    const IShaderBinaryStore* m_store = nullptr;
};

} // namespace oryx
