#include "oxpch.h"
#include "Oryx/Assets/GpuAssetCache.h"

namespace oryx
{

const Texture2D& GpuAssetCache::get(IRHI& rhi, const AssetManager& assets, AssetHandle<ImageAsset> handle)
{
    const ImageAsset& image = assets.get(handle);
    std::unordered_map<uint32_t, Entry>::iterator found = m_entries.find(handle.id.value);
    if (found != m_entries.end() && found->second.generation == handle.generation)
    {
        return found->second.texture;
    }

    Texture2D texture = Texture2D::create(rhi, { .width = static_cast<uint32_t>(image.width()),
                                                 .height = static_cast<uint32_t>(image.height()),
                                                 .pixels = image.pixels().data(),
                                                 .pixel_bytes = static_cast<uint32_t>(image.pixels().size()) });
    if (found != m_entries.end())
    {
        found->second = Entry{ handle.generation, std::move(texture) };
        return found->second.texture;
    }
    return m_entries.emplace(handle.id.value, Entry{ handle.generation, std::move(texture) }).first->second.texture;
}

void GpuAssetCache::release(AssetHandle<ImageAsset> handle)
{
    std::unordered_map<uint32_t, Entry>::iterator found = m_entries.find(handle.id.value);
    if (found != m_entries.end() && found->second.generation == handle.generation)
    {
        m_entries.erase(found);
    }
}

void GpuAssetCache::trim(const AssetManager& assets)
{
    for (std::unordered_map<uint32_t, Entry>::iterator it = m_entries.begin(); it != m_entries.end();)
    {
        it = assets.alive(AssetHandle<ImageAsset>{ AssetId{ it->first }, it->second.generation }) ? std::next(it) : m_entries.erase(it);
    }
}

void GpuAssetCache::clear()
{
    m_entries.clear();
}

bool GpuAssetCache::contains(AssetHandle<ImageAsset> handle) const
{
    std::unordered_map<uint32_t, Entry>::const_iterator found = m_entries.find(handle.id.value);
    return found != m_entries.end() && found->second.generation == handle.generation;
}

} // namespace oryx
