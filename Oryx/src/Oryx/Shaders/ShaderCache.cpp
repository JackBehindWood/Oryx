#include "oxpch.h"
#include "Oryx/Shaders/ShaderCache.h"

#include "Oryx/Shaders/ShaderHash.h"

namespace oryx
{

const ShaderCompilerOutput* ShaderCache::find(ShaderHash hash) const
{
    const std::unordered_map<ShaderHash, ShaderCompilerOutput>::const_iterator it = m_entries.find(hash);
    return it == m_entries.end() ? nullptr : &it->second;
}

const ShaderCompilerOutput& ShaderCache::get_or_compile(const ShaderCompilerInput& input)
{
    const IShaderCompiler& compiler = shader_compiler_for(input.source.language);
    const ShaderHash hash = hash_shader_input(input, compiler.id(), compiler.version());
    if (const ShaderCompilerOutput* cached = find(hash))
    {
        ++m_hits;
        return *cached;
    }
    ++m_misses;
    ShaderCompilerOutput output = compiler.compile(input);
    output.hash = hash;
    return m_entries.emplace(hash, std::move(output)).first->second;
}

void ShaderCache::clear()
{
    m_entries.clear();
    m_hits = 0;
    m_misses = 0;
}

ShaderCacheStats ShaderCache::stats() const
{
    return { m_hits, m_misses, static_cast<uint32_t>(m_entries.size()) };
}

} // namespace oryx
