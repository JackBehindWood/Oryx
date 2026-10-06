#include "oxpch.h"
#include "Oryx/Assets/Types/ImageAsset.h"

#include "Oryx/Core/Error.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace oryx
{

ImageAsset ImageAsset::decode(const uint8_t* encoded, size_t size)
{
    int32_t width = 0;
    int32_t height = 0;
    int32_t source_channels = 0;
    uint8_t* decoded = stbi_load_from_memory(encoded, static_cast<int32_t>(size), &width, &height, &source_channels, 4);
    if (decoded == nullptr)
    {
        throw Error("cannot decode image", stbi_failure_reason());
    }
    std::vector<uint8_t> pixels(decoded, decoded + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(decoded);
    return ImageAsset(width, height, std::move(pixels));
}

ImageAsset ImageAsset::from_rgba8(int32_t width, int32_t height, std::vector<uint8_t> pixels)
{
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4)
    {
        throw Error("invalid image pixel data");
    }
    return ImageAsset(width, height, std::move(pixels));
}

} // namespace oryx
