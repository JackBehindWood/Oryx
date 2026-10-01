#pragma once

#include "Oryx/Core/Registry.h"

namespace oryx
{

template<typename T>
class IAssetLoader
{
public:
    virtual ~IAssetLoader() = default;

    [[nodiscard]] virtual UniquePtr<T> load(const std::filesystem::path& path) const = 0;
};

} // namespace oryx

#define OX_REGISTER_ASSET_LOADER(AssetType, LoaderType, extension) \
    OX_REGISTER_FACTORY(::oryx::IAssetLoader<AssetType>, LoaderType, extension)
