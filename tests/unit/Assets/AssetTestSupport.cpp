#include "doctest.h"

#include "AssetTestSupport.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace oryx::test
{

std::vector<uint8_t> encode_png(int32_t width, int32_t height, int32_t channels, const std::vector<uint8_t>& pixels)
{
    std::vector<uint8_t> encoded;
    stbi_write_png_to_func(
        [](void* context, void* data, int32_t size)
        {
            std::vector<uint8_t>* out = static_cast<std::vector<uint8_t>*>(context);
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            out->insert(out->end(), bytes, bytes + size);
        },
        &encoded, width, height, channels, pixels.data(), width * channels);
    return encoded;
}

void write_bytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void write_png(const std::filesystem::path& path, int32_t width, int32_t height, int32_t channels, const std::vector<uint8_t>& pixels)
{
    write_bytes(path, encode_png(width, height, channels, pixels));
}

} // namespace oryx::test
