#include "doctest.h"

#include "Oryx.h"

#include <clocale>

using namespace oryx;

namespace
{

struct Record
{
    int32_t tag;
    float value;
};

std::vector<float> read(const Values& view)
{
    std::vector<float> out;
    for (uint32_t index = 0; index < view.count; ++index)
    {
        out.push_back(value_at(view, index));
    }
    return out;
}

} // namespace

TEST_CASE("GUI policies: an array, a ring and a strided field read the same sequence")
{
    const float array[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    const float ring[4] = { 3.0f, 4.0f, 1.0f, 2.0f };
    const Record records[4] = { { 0, 1.0f }, { 0, 2.0f }, { 0, 3.0f }, { 0, 4.0f } };
    const std::vector<float> expected = { 1.0f, 2.0f, 3.0f, 4.0f };
    CHECK(read(values(array)) == expected);
    CHECK(read(values_ring(ring, 4, 2)) == expected);
    CHECK(read(values_strided(&records[0].value, 4, sizeof(Record))) == expected);
    CHECK(read(values_ring(ring, 4, 6)) == expected);
}

TEST_CASE("GUI policies: empty and out-of-range reads are NaN gaps")
{
    const float array[2] = { 1.0f, 2.0f };
    CHECK(std::isnan(value_at(Values{}, 0)));
    CHECK(std::isnan(value_at(values(array), 2)));
    CHECK(std::isnan(value_at(values_ring(array, 0, 3), 0)));
    CHECK(values_ring(array, 0, 3).offset == 0);
}

TEST_CASE("GUI policies: numbers format in the C locale in every style")
{
    FrameArena arena;
    CHECK(format_number({ NumberStyle::General, 3 }, 1234.5f, arena) == "1.23e+03");
    CHECK(format_number({ NumberStyle::General, 3 }, 0.5f, arena) == "0.5");
    CHECK(format_number({ NumberStyle::Fixed, 2 }, 1.5f, arena) == "1.50");
    CHECK(format_number({ NumberStyle::Percent, 0 }, 0.25f, arena) == "25%");
    CHECK(format_number({ NumberStyle::Integer, 0 }, 2.6f, arena) == "3");
    CHECK(format_number({}, std::numeric_limits<float>::quiet_NaN(), arena) == "-");
    CHECK(format_number({}, std::numeric_limits<float>::infinity(), arena) == "-");

    const char* previous = std::setlocale(LC_NUMERIC, nullptr);
    const std::string saved = previous != nullptr ? previous : "C";
    if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") != nullptr || std::setlocale(LC_NUMERIC, "de_DE") != nullptr)
    {
        CHECK(format_number({ NumberStyle::Fixed, 2 }, 1.5f, arena) == "1.50");
    }
    std::setlocale(LC_NUMERIC, saved.c_str());
}

TEST_CASE("GUI policies: a colour scale hits its stops, clamps and marks missing data")
{
    const ColourScale scale = sequential_scale(0.0f, 10.0f);
    CHECK(approx_equal(evaluate(scale, 0.0f), scale.low));
    CHECK(approx_equal(evaluate(scale, 5.0f), scale.mid));
    CHECK(approx_equal(evaluate(scale, 10.0f), scale.high));
    CHECK(approx_equal(evaluate(scale, -4.0f), scale.low));
    CHECK(approx_equal(evaluate(scale, 99.0f), scale.high));
    CHECK(approx_equal(evaluate(scale, std::numeric_limits<float>::quiet_NaN()), scale.no_data));
    const ColourScale diverging = diverging_scale(-1.0f, 1.0f);
    CHECK(approx_equal(evaluate(diverging, 0.0f), diverging.mid));
    CHECK(evaluate(diverging, -1.0f).b > evaluate(diverging, -1.0f).r);
    CHECK(evaluate(diverging, 1.0f).r > evaluate(diverging, 1.0f).b);
    CHECK(approx_equal(evaluate(sequential_scale(3.0f, 3.0f), 7.0f), scale.low));
}

TEST_CASE("GUI policies: plot coordinates round trip and the palette wraps")
{
    PlotArea area;
    area.rect = { { 10.0f, 20.0f }, { 100.0f, 50.0f } };
    area.x_max = 10.0f;
    area.y_min = 0.0f;
    area.y_max = 5.0f;
    const Vec2f top_right = to_screen(area, 10.0f, 5.0f);
    CHECK(top_right[0] == doctest::Approx(110.0f));
    CHECK(top_right[1] == doctest::Approx(20.0f));
    const Vec2f bottom_left = to_screen(area, 0.0f, 0.0f);
    CHECK(bottom_left[0] == doctest::Approx(10.0f));
    CHECK(bottom_left[1] == doctest::Approx(70.0f));
    CHECK(index_at(area, 60.0f) == doctest::Approx(5.0f));

    GuiTheme theme;
    CHECK(palette_colour(theme, 0) == theme.palette[0]);
    CHECK(palette_colour(theme, k_palette_size + 2) == theme.palette[2]);
    CHECK(theme.palette[0] != theme.palette[1]);
}
