#include "oxpch.h"
#include "Oryx/Assets/GpuShaderStore.h"

#include "Oryx/Assets/Assets.h"

namespace oryx
{

namespace
{

constexpr uint32_t SHADER_STORE_VERSION = 1;
constexpr const char* BINARY_TYPE = "shader";
constexpr const char* MAP_TYPE = "shadermap";

} // namespace

OX_REGISTER_COMPILED_TYPE(BINARY_TYPE, SHADER_STORE_VERSION)
OX_REGISTER_COMPILED_TYPE(MAP_TYPE, SHADER_STORE_VERSION)

CompiledAssetKey GpuShaderStore::entry_key(ShaderStoreKind kind, uint64_t key)
{
    return CompiledAssetKey{ kind == ShaderStoreKind::Binary ? BINARY_TYPE : MAP_TYPE, SHADER_STORE_VERSION, 0, key };
}

bool GpuShaderStore::read(ShaderStoreKind kind, uint64_t key, std::vector<uint8_t>& payload) const
{
    return m_store.read(entry_key(kind, key), payload);
}

void GpuShaderStore::write(ShaderStoreKind kind, uint64_t key, const uint8_t* payload, size_t size) const
{
    m_store.write(entry_key(kind, key), payload, size);
}

const IShaderBinaryStore& gpu_shader_store()
{
    // Rebuilt when the manager changes so the reference never outlives the store it wraps.
    static const CompiledAssetStore* bound = nullptr;
    static UniquePtr<GpuShaderStore> adapter;
    const CompiledAssetStore* current = &Assets::manager().compiled();
    if (adapter == nullptr || bound != current)
    {
        adapter = create_unique<GpuShaderStore>(*current);
        bound = current;
    }
    return *adapter;
}

} // namespace oryx
