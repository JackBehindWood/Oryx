#pragma once

#include "Oryx.h"

namespace oryx::test
{

class AssetTempDir
{
public:
    AssetTempDir()
    {
        Random random;
        std::filesystem::path base = std::filesystem::temp_directory_path() / ("oryx-assets-" + std::to_string(random.get_int()));
        std::filesystem::create_directories(base);
        m_path = std::filesystem::weakly_canonical(base);
    }

    ~AssetTempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    AssetTempDir(const AssetTempDir&) = delete;
    AssetTempDir& operator=(const AssetTempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

inline AssetSettings uncached_settings()
{
    AssetSettings settings;
    settings.cache_enabled = false;
    return settings;
}

template<typename T>
AssetHandle<T> load_now(AssetManager& manager, const std::filesystem::path& path)
{
    AssetHandle<T> handle = manager.load<T>(path);
    manager.wait(handle);
    return handle;
}

void write_png(const std::filesystem::path& path, int32_t width, int32_t height, int32_t channels, const std::vector<uint8_t>& pixels);
std::vector<uint8_t> encode_png(int32_t width, int32_t height, int32_t channels, const std::vector<uint8_t>& pixels);
void write_bytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes);

} // namespace oryx::test
