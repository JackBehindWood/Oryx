#include "oxpch.h"
#include "Oryx/Assets/Import/AssetCache.h"

#include "Oryx/Core/Fnv.h"
#include "Oryx/Core/Log.h"

namespace oryx
{

namespace
{

constexpr uint32_t MAGIC = 0x4F584143;
constexpr uint32_t FORMAT_VERSION = 1;

struct Header
{
    uint32_t magic;
    uint32_t format_version;
    uint64_t length;
    uint64_t checksum;
};

uint64_t checksum_of(const uint8_t* payload, size_t size)
{
    Fnv1a hash;
    hash.mix(payload, size);
    return hash.value();
}

std::string hex(uint64_t value)
{
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

} // namespace

AssetCacheKey make_asset_cache_key(std::string_view id, uint32_t version, uint64_t settings_hash, const uint8_t* source, size_t source_size)
{
    Fnv1a hash;
    hash.mix(source, source_size);
    return AssetCacheKey{ std::string(id), version, settings_hash, hash.value() };
}

uint64_t hash_asset_cache_key(const AssetCacheKey& key)
{
    Fnv1a hash;
    hash.mix_string(key.id);
    hash.mix_value(key.version);
    hash.mix_value(key.settings_hash);
    hash.mix_value(key.source_hash);
    return hash.value();
}

AssetCache::AssetCache(std::filesystem::path directory, bool enabled)
    : m_directory(std::move(directory))
    , m_enabled(enabled)
{
}

std::filesystem::path AssetCache::entry_path(const AssetCacheKey& key) const
{
    return m_directory / key.id / (hex(hash_asset_cache_key(key)) + ".bin");
}

bool AssetCache::read(const AssetCacheKey& key, std::vector<uint8_t>& payload) const
{
    if (!m_enabled)
    {
        return false;
    }
    std::ifstream file(entry_path(key), std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    Header header{};
    if (size < static_cast<std::streamsize>(sizeof(Header)) || !file.read(reinterpret_cast<char*>(&header), sizeof(Header)))
    {
        return false;
    }
    if (header.magic != MAGIC || header.format_version != FORMAT_VERSION || header.length != static_cast<uint64_t>(size) - sizeof(Header))
    {
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(header.length));
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
    {
        return false;
    }
    if (checksum_of(bytes.data(), bytes.size()) != header.checksum)
    {
        return false;
    }
    payload = std::move(bytes);
    return true;
}

void AssetCache::write(const AssetCacheKey& key, const uint8_t* payload, size_t size) const
{
    if (!m_enabled)
    {
        return;
    }
    std::filesystem::path target = entry_path(key);
    std::filesystem::path temp = target;
    temp += ".tmp" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    bool written = false;
    if (!error)
    {
        Header header{ MAGIC, FORMAT_VERSION, size, checksum_of(payload, size) };
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(&header), sizeof(Header));
        file.write(reinterpret_cast<const char*>(payload), static_cast<std::streamsize>(size));
        file.close();
        written = static_cast<bool>(file);
        if (written)
        {
            std::filesystem::rename(temp, target, error);
            written = !error;
        }
        if (!written)
        {
            std::filesystem::remove(temp, error);
        }
    }
    if (!written && !m_warned.exchange(true))
    {
        OX_WARN("asset cache directory '{}' is not writable; continuing without caching", m_directory.generic_string());
    }
}

} // namespace oryx
