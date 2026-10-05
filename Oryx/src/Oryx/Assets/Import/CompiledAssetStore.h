#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// The path is excluded on purpose so moved or renamed sources still hit.
struct AssetCacheKey
{
    std::string id;
    uint32_t version = 0;
    uint64_t settings_hash = 0;
    uint64_t source_hash = 0;
};

[[nodiscard]] AssetCacheKey make_asset_cache_key(std::string_view id, uint32_t version, uint64_t settings_hash, const uint8_t* source, size_t source_size);
[[nodiscard]] uint64_t hash_asset_cache_key(const AssetCacheKey& key);

// Disposable derived-data store: <dir>/<id>/<hex>.bin. A corrupt or truncated entry is a miss; an unwritable directory warns once.
class AssetCache
{
public:
    AssetCache(std::filesystem::path directory, bool enabled);

    [[nodiscard]] bool read(const AssetCacheKey& key, std::vector<uint8_t>& payload) const;
    void write(const AssetCacheKey& key, const uint8_t* payload, size_t size) const;

    [[nodiscard]] std::filesystem::path entry_path(const AssetCacheKey& key) const;
    [[nodiscard]] bool enabled() const { return m_enabled; }

private:
    std::filesystem::path m_directory;
    bool m_enabled;
    mutable std::atomic<bool> m_warned{ false };
};

} // namespace oryx
