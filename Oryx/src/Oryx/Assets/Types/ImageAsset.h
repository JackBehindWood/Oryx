#pragma once

#include "Oryx/Assets/Asset.h"

namespace oryx
{

class ImageAsset : public Asset
{
public:
    [[nodiscard]] static ImageAsset decode(const uint8_t* encoded, size_t size);
    [[nodiscard]] static ImageAsset from_rgba8(int32_t width, int32_t height, std::vector<uint8_t> pixels);

    [[nodiscard]] int32_t width() const { return m_width; }
    [[nodiscard]] int32_t height() const { return m_height; }
    [[nodiscard]] const uint8_t* pixels() const { return m_pixels.data(); }
    [[nodiscard]] size_t pixel_bytes() const { return m_pixels.size(); }
    [[nodiscard]] size_t memory_bytes() const override { return m_pixels.size(); }

private:
    ImageAsset(int32_t width, int32_t height, std::vector<uint8_t> pixels)
        : m_width(width)
        , m_height(height)
        , m_pixels(std::move(pixels))
    {
    }

    int32_t m_width;
    int32_t m_height;
    std::vector<uint8_t> m_pixels;
};

} // namespace oryx
