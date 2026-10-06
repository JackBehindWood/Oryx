#include "oxpch.h"
#include "Oryx/Shaders/Cache/ShaderCache.h"

#include "Oryx/Shaders/Cache/ShaderHash.h"

namespace oryx
{

const ShaderCompilerOutput* ShaderCache::find(ShaderHash hash) const
{
    const std::unordered_map<ShaderHash, ShaderCompilerOutput>::const_iterator it = m_entries.find(hash);
    return it == m_entries.end() ? nullptr : &it->second;
}

const ShaderCompilerOutput& ShaderCache::get_or_compile(const ShaderCompilerInput& input)
{
    if (input.source.language == ShaderLanguage::Slang)
    {
        return get_or_compile(input, SlangCompiler(m_slang));
    }
    return get_or_compile(input, shader_compiler_for(input.source.language));
}

const ShaderCompilerOutput& ShaderCache::get_or_compile(const ShaderCompilerInput& input, const IShaderCompiler& compiler)
{
    const ShaderHash hash = hash_shader_input(input, compiler);
    if (const ShaderCompilerOutput* cached = find(hash))
    {
        ++m_hits;
        return *cached;
    }
    if (m_store != nullptr)
    {
        ShaderCompilerOutput stored;
        if (read_shader_output(*m_store, hash, stored))
        {
            ++m_store_hits;
            return m_entries.emplace(hash, std::move(stored)).first->second;
        }
    }
    ++m_misses;
    ShaderCompilerOutput output = compiler.compile(input);
    output.hash = hash;
    output.language = input.source.language;
    output.compiler_id = compiler.id();
    output.compiler_version = compiler.version();
    if (m_store != nullptr)
    {
        write_shader_output(*m_store, hash, output);
    }
    return m_entries.emplace(hash, std::move(output)).first->second;
}

void ShaderCache::clear()
{
    m_entries.clear();
    m_hits = 0;
    m_misses = 0;
    m_store_hits = 0;
}

ShaderCacheStats ShaderCache::stats() const
{
    return { m_hits, m_misses, m_store_hits, static_cast<uint32_t>(m_entries.size()) };
}

} // namespace oryx
