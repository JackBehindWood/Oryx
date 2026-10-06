#pragma once

#include "Oryx/Assets/AssetManager.h"
#include "Oryx/Assets/Types/ImageAsset.h"
#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Graphics/Resources/Texture2D.h"

namespace oryx
{

// Uploads decoded images to the GPU once and hands back the same Texture2D until the asset's generation or revision changes. Main thread only.
class GpuAssetCache
{
public:
    // A 1x1 magenta placeholder while the asset is Loading or Failed. Throws Error when the handle is stale.
    // The reference is valid until the next get, release, trim or clear.
    [[nodiscard]] const Texture2D& get(IRHI& rhi, const AssetManager& assets, AssetHandle<ImageAsset> handle);

    void release(AssetHandle<ImageAsset> handle);
    // Drops every texture whose asset is no longer alive in `assets`.
    void trim(const AssetManager& assets);
    void clear();

    [[nodiscard]] bool contains(AssetHandle<ImageAsset> handle) const;
    [[nodiscard]] size_t size() const { return m_entries.size(); }

private:
    struct Entry
    {
        uint32_t generation;
        uint32_t revision;
        Texture2D texture;
    };

    std::unordered_map<uint32_t, Entry> m_entries;
    UniquePtr<Texture2D> m_placeholder;
};

} // namespace oryx
