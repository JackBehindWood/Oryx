#include "oxpch.h"
#include "Oryx/Assets/AssetManager.h"

namespace oryx
{

std::string AssetManager::path_key(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string AssetManager::extension_key(const std::filesystem::path& path)
{
    std::string extension = path.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

} // namespace oryx
