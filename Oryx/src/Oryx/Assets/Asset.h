#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// Base of every type AssetManager owns; the manager fills in the source path when it publishes the asset.
class Asset
{
public:
    virtual ~Asset() = default;

    [[nodiscard]] const std::filesystem::path& source_path() const { return m_source_path; }
    [[nodiscard]] virtual size_t memory_bytes() const = 0;

protected:
    Asset() = default;
    Asset(Asset&&) = default;
    Asset& operator=(Asset&&) = default;

private:
    friend class AssetManager;

    std::filesystem::path m_source_path;
};

} // namespace oryx
