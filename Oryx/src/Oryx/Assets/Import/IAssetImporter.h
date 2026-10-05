#pragma once

#include "Oryx/Assets/Import/CompiledAssetStore.h"
#include "Oryx/Core/Registry.h"

namespace oryx
{

struct ImportInput
{
    std::filesystem::path path;
    const uint8_t* bytes = nullptr;
    size_t size = 0;
};

// Turns source bytes into a cacheable payload (import_source) and the payload into a runtime asset (instantiate).
template<typename T>
class IAssetImporter
{
public:
    virtual ~IAssetImporter() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;
    // Bump when import_source's output format or behaviour changes.
    [[nodiscard]] virtual uint32_t version() const = 0;
    [[nodiscard]] virtual uint64_t settings_hash() const { return 0; }

    // Pure and deterministic: no globals, no manager access, throws Error.
    [[nodiscard]] virtual std::vector<uint8_t> import_source(const ImportInput& input) const = 0;
    // Runs on the main thread; may load other assets.
    [[nodiscard]] virtual UniquePtr<T> instantiate(const uint8_t* payload, size_t size) const = 0;
};

} // namespace oryx

#define OX_REGISTER_ASSET_IMPORTER(AssetType, ImporterType, extension) \
    OX_REGISTER_FACTORY(::oryx::IAssetImporter<AssetType>, ImporterType, extension) \
    namespace                                                                       \
    {                                                                               \
    [[maybe_unused]] const int OX_CONCAT(g_ox_register_compiled_importer_, __LINE__) = (::oryx::register_compiled_type(ImporterType{}.id(), [] { return ImporterType{}.version(); }), 0); \
    }
