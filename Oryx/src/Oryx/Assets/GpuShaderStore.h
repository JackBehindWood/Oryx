#pragma once

#include "Oryx/Assets/Import/CompiledAssetStore.h"
#include "Oryx/Shaders/Cache/ShaderBinaryStore.h"

namespace oryx
{

// Persists compiled shaders and the cooked shader map in the compiled-asset store: <compiled>/shader/<hash16>.oxshader and
// <compiled>/shadermap/<id16>.oxshadermap. The one place the Shaders module's store interface meets Assets storage.
class GpuShaderStore final : public IShaderBinaryStore
{
public:
    explicit GpuShaderStore(const CompiledAssetStore& store)
        : m_store(store)
    {
    }

    [[nodiscard]] bool read(ShaderStoreKind kind, uint64_t key, std::vector<uint8_t>& payload) const override;
    void write(ShaderStoreKind kind, uint64_t key, const uint8_t* payload, size_t size) const override;

    [[nodiscard]] static CompiledAssetKey entry_key(ShaderStoreKind kind, uint64_t key);

private:
    const CompiledAssetStore& m_store;
};

// A store over Assets::manager().compiled(), created on first use; valid until Assets::reset().
[[nodiscard]] const IShaderBinaryStore& gpu_shader_store();

} // namespace oryx
