#include "oxpch.h"
#include "Oryx/Assets/Import/IAssetImporter.h"

#include "Oryx/Assets/Types/FontAsset.h"

namespace oryx
{

namespace
{

class FontAssetImporter : public IAssetImporter<FontAsset>
{
public:
    std::string_view id() const override { return "font"; }
    uint32_t version() const override { return 1; }

    std::vector<uint8_t> import_source(const ImportInput& input) const override
    {
        std::vector<uint8_t> bytes(input.bytes, input.bytes + input.size);
        (void)FontAsset::from_bytes(bytes);
        return bytes;
    }

    UniquePtr<FontAsset> instantiate(const uint8_t* payload, size_t size) const override
    {
        return create_unique<FontAsset>(FontAsset::from_bytes(std::vector<uint8_t>(payload, payload + size)));
    }
};

} // namespace

OX_REGISTER_ASSET_IMPORTER(FontAsset, FontAssetImporter, ".ttf")

} // namespace oryx
