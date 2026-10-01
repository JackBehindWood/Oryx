#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

class ImageAsset
{
public:
    [[nodiscard]] static ImageAsset decode(std::span<const uint8_t> encoded);

    [[nodiscard]] int32_t width() const { return m_width; }
    [[nodiscard]] int32_t height() const { return m_height; }
    [[nodiscard]] std::span<const uint8_t> pixels() const { return m_pixels; }

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
