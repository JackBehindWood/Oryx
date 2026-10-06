#pragma once

#include "Oryx/Shaders/ShaderCache.h"
#include "Oryx/Shaders/ShaderSourceProvider.h"
#include "Oryx/Shaders/ShaderType.h"

namespace oryx
{

class IRHI;

// Holds the compiled instance of every registered shader permutation; an owned value, never global.
class ShaderLibrary
{
public:
    // Compiles every permutation the type accepts, loading type.source (a virtual path) and its includes through `sources`; the
    // overloads without one read the embedded copies. Throws Error naming the type when a load or compile fails.
    void compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type, const IShaderSourceProvider& sources);
    void compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type);
    void compile_all(IRHI& rhi, ShaderCache& cache, const IShaderSourceProvider& sources);
    void compile_all(IRHI& rhi, ShaderCache& cache);

    // Recompiles the type from `sources`; on any failure the previous shaders stay in place and the Error propagates.
    void reload(IRHI& rhi, ShaderCache& cache, const ShaderType& type, const IShaderSourceProvider& sources);
    void reload_all(IRHI& rhi, ShaderCache& cache, const IShaderSourceProvider& sources);

    // Throws Error when the permutation was not compiled.
    template<typename T>
    [[nodiscard]] Ref<T> get(uint32_t permutation = 0) const
    {
        return Ref<T>::from_raw(static_cast<T*>(find(typeid(T), permutation).get()));
    }

    template<typename T>
    [[nodiscard]] bool contains(uint32_t permutation = 0) const
    {
        return m_shaders.count({ std::type_index(typeid(T)), permutation }) != 0;
    }

    [[nodiscard]] size_t size() const { return m_shaders.size(); }
    void clear() { m_shaders.clear(); }

private:
    [[nodiscard]] const ShaderPtr& find(std::type_index type, uint32_t permutation) const;

    std::map<std::pair<std::type_index, uint32_t>, ShaderPtr> m_shaders;
};

} // namespace oryx
