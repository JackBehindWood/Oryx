#include "oxpch.h"
#include "Oryx/Assets/ImageAsset.h"

#include "Oryx/Assets/AssetFile.h"
#include "Oryx/Assets/AssetManager.h"
#include "Oryx/Core/Error.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace oryx
{

ImageAsset ImageAsset::decode(std::span<const uint8_t> encoded)
{
    int32_t width = 0;
    int32_t height = 0;
    int32_t source_channels = 0;
    uint8_t* decoded = stbi_load_from_memory(encoded.data(), static_cast<int32_t>(encoded.size()), &width, &height, &source_channels, 4);
    if (decoded == nullptr)
    {
        throw Error("cannot decode image", stbi_failure_reason());
    }
    std::vector<uint8_t> pixels(decoded, decoded + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(decoded);
    return ImageAsset(width, height, std::move(pixels));
}

namespace
{

class ImageAssetLoader : public IAssetLoader<ImageAsset>
{
public:
    UniquePtr<ImageAsset> load(const std::filesystem::path& path) const override
    {
        return create_unique<ImageAsset>(ImageAsset::decode(read_binary_file(path)));
    }
};

} // namespace

OX_REGISTER_ASSET_LOADER(ImageAsset, ImageAssetLoader, ".png")
OX_REGISTER_ASSET_LOADER(ImageAsset, ImageAssetLoader, ".jpg")
OX_REGISTER_ASSET_LOADER(ImageAsset, ImageAssetLoader, ".jpeg")
OX_REGISTER_ASSET_LOADER(ImageAsset, ImageAssetLoader, ".bmp")
OX_REGISTER_ASSET_LOADER(ImageAsset, ImageAssetLoader, ".tga")

} // namespace oryx
