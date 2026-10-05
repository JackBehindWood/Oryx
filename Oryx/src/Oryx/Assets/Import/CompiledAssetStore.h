#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// The type is the importer id (or baker name): lowercase letters and digits, at most 19 characters. The source path is excluded on purpose so moved or renamed sources still hit.
struct CompiledAssetKey
{
    std::string type;
    uint32_t version = 0;
    uint64_t settings_hash = 0;
    uint64_t source_hash = 0;
};

// Throws Error for a type that is not path-safe.
[[nodiscard]] CompiledAssetKey make_compiled_asset_key(std::string_view type, uint32_t version, uint64_t settings_hash, const uint8_t* source, size_t source_size);
[[nodiscard]] uint64_t hash_compiled_asset_key(const CompiledAssetKey& key);

// The current version of every compiled type, read when pruning; importers and bakers register on static init.
void register_compiled_type(std::string_view type, std::function<uint32_t()> current_version);
[[nodiscard]] std::map<std::string, uint32_t> compiled_type_versions();

// Disposable derived-data store: <dir>/<type>/<hex>.ox<type>. A corrupt, truncated or foreign entry is a miss; an unwritable directory warns once.
class CompiledAssetStore
{
public:
    CompiledAssetStore(std::filesystem::path directory, bool enabled);

    [[nodiscard]] bool read(const CompiledAssetKey& key, std::vector<uint8_t>& payload) const;
    void write(const CompiledAssetKey& key, const uint8_t* payload, size_t size) const;

    // Removes entries no registered type can read: other format version, unknown type, stale importer version, bad header, leftover temp files.
    void prune() const;

    [[nodiscard]] std::filesystem::path entry_path(const CompiledAssetKey& key) const;
    [[nodiscard]] bool enabled() const { return m_enabled; }

private:
    std::filesystem::path m_directory;
    bool m_enabled;
    mutable std::atomic<bool> m_warned{ false };
};

} // namespace oryx

#define OX_REGISTER_COMPILED_TYPE(type, version) \
    namespace                                    \
    {                                            \
    [[maybe_unused]] const int OX_CONCAT(g_ox_register_compiled_type_, __LINE__) = (::oryx::register_compiled_type(type, [] { return static_cast<uint32_t>(version); }), 0); \
    }
