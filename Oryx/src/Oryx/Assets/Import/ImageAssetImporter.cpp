#include "oxpch.h"
#include "Oryx/Assets/Import/IAssetImporter.h"

#include "Oryx/Assets/Types/ImageAsset.h"
#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

constexpr size_t PAYLOAD_HEADER = 8;

class ImageAssetImporter : public IAssetImporter<ImageAsset>
{
public:
    std::string_view id() const override { return "image"; }
    uint32_t version() const override { return 1; }

    std::vector<uint8_t> import_source(const ImportInput& input) const override
    {
        ImageAsset image = ImageAsset::decode(input.bytes, input.size);
        uint32_t header[2] = { static_cast<uint32_t>(image.width()), static_cast<uint32_t>(image.height()) };
        std::vector<uint8_t> payload(PAYLOAD_HEADER + image.pixel_bytes());
        std::memcpy(payload.data(), header, PAYLOAD_HEADER);
        std::memcpy(payload.data() + PAYLOAD_HEADER, image.pixels(), image.pixel_bytes());
        return payload;
    }

    UniquePtr<ImageAsset> instantiate(const uint8_t* payload, size_t size) const override
    {
        if (size < PAYLOAD_HEADER)
        {
            throw Error("imported image payload is truncated");
        }
        uint32_t header[2];
        std::memcpy(header, payload, PAYLOAD_HEADER);
        return create_unique<ImageAsset>(ImageAsset::from_rgba8(static_cast<int32_t>(header[0]), static_cast<int32_t>(header[1]),
            std::vector<uint8_t>(payload + PAYLOAD_HEADER, payload + size)));
    }
};

} // namespace

OX_REGISTER_ASSET_IMPORTER(ImageAsset, ImageAssetImporter, ".png")
OX_REGISTER_ASSET_IMPORTER(ImageAsset, ImageAssetImporter, ".jpg")
OX_REGISTER_ASSET_IMPORTER(ImageAsset, ImageAssetImporter, ".jpeg")
OX_REGISTER_ASSET_IMPORTER(ImageAsset, ImageAssetImporter, ".bmp")
OX_REGISTER_ASSET_IMPORTER(ImageAsset, ImageAssetImporter, ".tga")

} // namespace oryx
