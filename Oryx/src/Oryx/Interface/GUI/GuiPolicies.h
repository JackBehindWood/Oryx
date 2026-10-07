#pragma once

#include "Oryx/Interface/Canvas/FrameArena.h"
#include "Oryx/Interface/Canvas/Rect.h"
#include "Oryx/Interface/GUI/GuiTheme.h"
#include "Oryx/Math/Colour.h"

// Plain data the data widgets take instead of callbacks: a view of numbers, how to print one, how to colour one. Custom behaviour is the caller's own code (format the text, draw between PlotScope calls).
namespace oryx
{

// A read-only view of floats in the caller's memory: an array, a ring buffer (`offset` is the index of the oldest sample) or one field of a record array (`stride` is the byte distance between samples).
// The memory must outlive the widget call. Out-of-range reads give NaN, which the widgets treat as a gap.
struct Values
{
    const float* data = nullptr;
    uint32_t count = 0;
    uint32_t offset = 0;
    uint32_t stride = sizeof(float);
};

static_assert(std::is_trivially_copyable_v<Values> && std::is_standard_layout_v<Values>);

[[nodiscard]] inline Values values(const float* data, uint32_t count)
{
    return { data, count, 0, sizeof(float) };
}

template<size_t N>
[[nodiscard]] Values values(const float (&array)[N])
{
    return values(array, static_cast<uint32_t>(N));
}

[[nodiscard]] inline Values values_ring(const float* data, uint32_t count, uint32_t oldest)
{
    return { data, count, count > 0 ? oldest % count : 0, sizeof(float) };
}

// `first` points at the field of the first record; `stride_bytes` is sizeof the record.
[[nodiscard]] inline Values values_strided(const float* first, uint32_t count, uint32_t stride_bytes)
{
    return { first, count, 0, stride_bytes };
}

[[nodiscard]] inline float value_at(const Values& view, uint32_t index)
{
    if (view.data == nullptr || index >= view.count)
    {
        return std::numeric_limits<float>::quiet_NaN();
    }
    const uint32_t slot = view.offset + index >= view.count ? view.offset + index - view.count : view.offset + index;
    return *reinterpret_cast<const float*>(reinterpret_cast<const std::byte*>(view.data) + static_cast<size_t>(slot) * view.stride);
}

enum class NumberStyle : uint8_t
{
    // %g with `precision` significant digits.
    General,
    // `precision` decimals.
    Fixed,
    // The value is a fraction: 0.25 prints as 25%, with `precision` decimals.
    Percent,
    // Rounded, no decimals.
    Integer
};

struct NumberFormat
{
    NumberStyle style = NumberStyle::General;
    uint8_t precision = 3;
};

static_assert(std::is_trivially_copyable_v<NumberFormat> && std::is_standard_layout_v<NumberFormat>);

// C-locale text in the frame arena; NaN and infinities print as "-".
[[nodiscard]] std::string_view format_number(const NumberFormat& format, float value, FrameArena& arena);

// A three-stop colour ramp over [min, max]: `low` at min, `mid` halfway, `high` at max; `no_data` for NaN. A sequential scale runs light to dark or dark to light, a diverging one is neutral in the middle.
struct ColourScale
{
    Colour low = { 0.267f, 0.005f, 0.329f, 1.0f };
    Colour mid = { 0.128f, 0.567f, 0.551f, 1.0f };
    Colour high = { 0.993f, 0.906f, 0.144f, 1.0f };
    Colour no_data = { 0.25f, 0.25f, 0.25f, 1.0f };
    float min = 0.0f;
    float max = 1.0f;
};

static_assert(std::is_trivially_copyable_v<ColourScale> && std::is_standard_layout_v<ColourScale>);

[[nodiscard]] Colour evaluate(const ColourScale& scale, float value);
// Dark purple to yellow through teal (viridis end points): readable in greyscale and for the common colour-vision deficiencies.
[[nodiscard]] ColourScale sequential_scale(float min = 0.0f, float max = 1.0f);
// Blue through near-white to orange: neutral at the middle of [min, max], safe for red-green deficiencies.
[[nodiscard]] ColourScale diverging_scale(float min = -1.0f, float max = 1.0f);

// The data rectangle of a plot in screen space, valid inside a PlotScope: x is the sample index, y the value.
struct PlotArea
{
    Rect rect;
    float x_min = 0.0f;
    float x_max = 1.0f;
    float y_min = 0.0f;
    float y_max = 1.0f;
};

static_assert(std::is_trivially_copyable_v<PlotArea> && std::is_standard_layout_v<PlotArea>);

[[nodiscard]] Vec2f to_screen(const PlotArea& area, float x, float y);
// The sample index under a screen x, fractional and not clamped.
[[nodiscard]] float index_at(const PlotArea& area, float screen_x);

// Series and categorical colours come from the theme's palette, wrapping after k_palette_size.
[[nodiscard]] const Colour& palette_colour(const GuiTheme& theme, uint32_t index);

} // namespace oryx
