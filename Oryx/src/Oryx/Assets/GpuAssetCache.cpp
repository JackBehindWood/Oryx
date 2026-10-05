#include "oxpch.h"
#include "Oryx/Assets/GpuAssetCache.h"

namespace oryx
{

const Texture2D& GpuAssetCache::get(IRHI& rhi, const AssetManager& assets, AssetHandle<ImageAsset> handle)
{
    const ImageAsset* ready = assets.try_get(handle);
    if (ready == nullptr)
    {
        if (!assets.alive(handle))
        {
            throw Error("stale or null asset handle");
        }
        if (m_placeholder == nullptr)
        {
            const uint8_t magenta[4] = { 255, 0, 255, 255 };
            m_placeholder = create_unique<Texture2D>(Texture2D::create(rhi, { .width = 1, .height = 1, .pixels = magenta, .pixel_bytes = 4 }));
        }
        return *m_placeholder;
    }
    const ImageAsset& image = *ready;
    uint32_t revision = assets.revision(handle);
    std::unordered_map<uint32_t, Entry>::iterator found = m_entries.find(handle.id.value);
    if (found != m_entries.end() && found->second.generation == handle.generation && found->second.revision == revision)
    {
        return found->second.texture;
    }

    Texture2D texture = Texture2D::create(rhi, { .width = static_cast<uint32_t>(image.width()),
                                                 .height = static_cast<uint32_t>(image.height()),
                                                 .pixels = image.pixels(),
                                                 .pixel_bytes = static_cast<uint32_t>(image.pixel_bytes()) });
    if (found != m_entries.end())
    {
        found->second = Entry{ handle.generation, revision, std::move(texture) };
        return found->second.texture;
    }
    return m_entries.emplace(handle.id.value, Entry{ handle.generation, revision, std::move(texture) }).first->second.texture;
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
    m_placeholder = nullptr;
}

bool GpuAssetCache::contains(AssetHandle<ImageAsset> handle) const
{
    std::unordered_map<uint32_t, Entry>::const_iterator found = m_entries.find(handle.id.value);
    return found != m_entries.end() && found->second.generation == handle.generation;
}

} // namespace oryx
