#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/Cache/ShaderCache.h"
#include "Oryx/Shaders/Source/ShaderSourceProvider.h"
#include "Oryx/Shaders/ShaderType.h"

namespace oryx
{

class IRHI;

// Loads the type's source and the closure of its includes through `sources`; throws Error naming the missing path.
// The caller sets the permutation's defines.
[[nodiscard]] ShaderCompilerInput load_shader_input(const ShaderType& type, const IShaderSourceProvider& sources);

// Holds the compiled instance of every registered shader permutation; an owned value, never global.
class ShaderLibrary
{
public:
    // With the fallback on, a pixel shader that fails its first compile is replaced by the magenta ErrorPS (same class, is_fallback() true) and the error is logged;
    // a failure with a previous version in place, and any vertex shader failure, still throws. Cooked loads never fall back.
    void set_error_fallback(bool enabled) { m_error_fallback = enabled; }

    // Compiles every permutation the type accepts, loading type.source (a virtual path) and its includes through `sources`; the
    // overloads without one read the embedded copies. Throws Error naming the type when a load or compile fails.
    void compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type, const IShaderSourceProvider& sources);
    void compile(IRHI& rhi, ShaderCache& cache, const ShaderType& type);
    void compile_all(IRHI& rhi, ShaderCache& cache, const IShaderSourceProvider& sources);
    void compile_all(IRHI& rhi, ShaderCache& cache);

    // Builds every shader from a cooked store with no sources or compiler: the shader map for the registered types names each binary.
    // Throws Error naming the type when the map is missing or stale or an entry is absent; the library is unchanged then.
    void load_cooked(IRHI& rhi, const IShaderBinaryStore& store);

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
    [[nodiscard]] ShaderPtr build_fallback(IRHI& rhi, ShaderCache& cache, const ShaderType& type, uint32_t permutation, const Error& cause) const;

    std::map<std::pair<std::type_index, uint32_t>, ShaderPtr> m_shaders;
    bool m_error_fallback = false;
};

} // namespace oryx
