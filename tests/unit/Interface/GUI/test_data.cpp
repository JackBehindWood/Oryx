#include "doctest.h"

#include "Oryx.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using gui::BarOptions;
using gui::HeatGridOptions;
using gui::HeatGridResult;
using gui::PlotOptions;
using gui::PlotResult;
using gui::PlotStyle;
using gui::SparklineOptions;
using test::GuiFixture;

namespace
{

const ImageCmd* first_image(const DrawList& list)
{
    const DrawChannel& channel = list.channel(0);
    return channel.images.empty() ? nullptr : &channel.images[0];
}

} // namespace

TEST_CASE("GUI images: an image fills its box and carries handle, uv, tint and radius")
{
    GuiFixture f;
    const auto body = [&]
    {
        gui::ImageOptions options;
        options.width = fixed(24.0f);
        options.height = fixed(16.0f);
        options.uv_max = { 0.5f, 1.0f };
        options.tint = { 1.0f, 0.5f, 0.25f, 1.0f };
        options.radius = im::circle_radius();
        std::ignore = gui::image("pic", ImageHandle{ 2 }, options);
    };
    f.driver.settle(f.frame_of(body));
    const ImageCmd* image = first_image(f.context.draw_list());
    REQUIRE(image != nullptr);
    CHECK(image->image.index == 2);
    CHECK(image->rect.size[0] == doctest::Approx(24.0f));
    CHECK(image->rect.size[1] == doctest::Approx(16.0f));
    CHECK(image->uv_max[0] == doctest::Approx(0.5f));
    CHECK(image->tint.g == doctest::Approx(0.5f));
    CHECK(image->radius.top_left == doctest::Approx(8.0f));
}

TEST_CASE("GUI images: a button reports a click, dims while held and a frameless one draws only the picture")
{
    GuiFixture f;
    uint32_t clicks = 0;
    bool frame = true;
    const auto body = [&]
    {
        gui::ImageButtonOptions options;
        options.frame = frame;
        clicks += gui::image_button("pic", k_single_image, options).clicked ? 1 : 0;
    };
    f.driver.settle(f.frame_of(body));
    const ImageCmd* idle = first_image(f.context.draw_list());
    REQUIRE(idle != nullptr);
    const float idle_tint = idle->tint.r;
    const Vec2f at = rect_centre(idle->rect);
    f.driver.move_to(at);
    f.driver.frame(f.frame_of(body));
    f.driver.press();
    f.driver.frame(f.frame_of(body));
    f.driver.frame(f.frame_of(body));
    CHECK(first_image(f.context.draw_list())->tint.r < idle_tint);
    f.driver.release();
    f.driver.frame(f.frame_of(body));
    CHECK(clicks == 1);

    frame = false;
    f.driver.leave();
    f.driver.settle(f.frame_of(body));
    CHECK(f.context.draw_list().channel(0).rects.empty());
    CHECK(f.context.draw_list().channel(0).rounded_rects.empty());
}

TEST_CASE("GUI images: warm frames allocate nothing")
{
    GuiFixture f;
    const auto body = [&]
    {
        std::ignore = gui::image("a", k_single_image);
        std::ignore = gui::image_button("b", ImageHandle{ 1 });
    };
    f.driver.run_frames(4, f.frame_of(body));
    const MemoryStats before = test::all_allocations();
    f.driver.run_frames(3, f.frame_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

namespace
{

// Counts the plot's drawn commands of one kind in channel 0 after the layout merged its channels.
uint32_t lines_in(const GuiFixture& f)
{
    return static_cast<uint32_t>(f.context.draw_list().channel(0).lines.size());
}

struct PlotFixture : GuiFixture
{
    PlotFixture() { driver.input().surface_size = { 200.0f, 100.0f }; }

    template<typename Body>
    void run(Body&& body)
    {
        driver.settle(frame_of(body));
        driver.run_frames(2, frame_of(body));
    }
};

PlotOptions plot_options()
{
    PlotOptions options;
    options.width = fixed(100.0f);
    options.height = fixed(50.0f);
    options.legend = false;
    options.range_labels = false;
    return options;
}

std::vector<float> ramp(uint32_t count)
{
    std::vector<float> data(count);
    for (uint32_t index = 0; index < count; ++index)
    {
        data[index] = static_cast<float>(index % 10);
    }
    return data;
}

} // namespace

TEST_CASE("GUI bar: the value is clamped to the max and a zero max is empty")
{
    GuiFixture f;
    float value = 5.0f;
    float max = 10.0f;
    const auto body = [&] { gui::bar("load", value, max); };
    f.driver.settle(f.column_of(body));
    const auto fill_width = [&]
    {
        float best = 0.0f;
        for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
        {
            const LayoutNode& node = f.context.layout().node(index);
            if (node.paint.has_fill && node.paint.fill == f.context.theme().base.accent)
            {
                best = math::max(best, node.rect.size[0]);
            }
        }
        return best;
    };
    const float half = fill_width();
    CHECK(half > 0.0f);
    value = 50.0f;
    f.driver.settle(f.column_of(body));
    CHECK(fill_width() == doctest::Approx(half * 2.0f));
    value = -3.0f;
    f.driver.settle(f.column_of(body));
    CHECK(fill_width() == doctest::Approx(0.0f));
    max = 0.0f;
    value = 5.0f;
    f.driver.settle(f.column_of(body));
    CHECK(fill_width() == doctest::Approx(0.0f));
    CHECK(f.find_text("5") != nullptr);
}

TEST_CASE("GUI bar: a colour scale colours the fill by value and custom text replaces the number")
{
    GuiFixture f;
    const ColourScale scale = sequential_scale(0.0f, 1.0f);
    const auto body = [&]
    {
        BarOptions options;
        options.scale = &scale;
        options.value_text = "seven of ten";
        gui::bar("load", 1.0f, 2.0f, options);
    };
    f.driver.settle(f.column_of(body));
    CHECK(f.find_text("seven of ten") != nullptr);
    bool found = false;
    for (uint32_t index = 0; index < f.context.layout().node_count(); ++index)
    {
        found = found || (f.context.layout().node(index).paint.has_fill && approx_equal(f.context.layout().node(index).paint.fill, scale.high));
    }
    CHECK(found);
}

TEST_CASE("GUI plot: a plot with no data or one sample draws a frame and at most a dot")
{
    PlotFixture f;
    const float one[1] = { 4.0f };
    std::vector<float> data;
    const auto body = [&] { std::ignore = gui::plot_lines("p", values(data.data(), static_cast<uint32_t>(data.size())), plot_options()); };
    f.run(body);
    CHECK(lines_in(f) == 0);
    CHECK_FALSE(f.context.draw_list().channel(0).rounded_rects.empty());
    const size_t empty_rects = f.context.draw_list().channel(0).rects.size();
    data.assign(one, one + 1);
    f.run(body);
    CHECK(lines_in(f) == 0);
    CHECK(f.context.draw_list().channel(0).rects.size() == empty_rects + 1);
}

TEST_CASE("GUI plot: a constant series is a flat line in the middle")
{
    PlotFixture f;
    const float flat[5] = { 3.0f, 3.0f, 3.0f, 3.0f, 3.0f };
    const auto body = [&] { std::ignore = gui::plot_lines("p", values(flat), plot_options()); };
    f.run(body);
    REQUIRE(lines_in(f) == 4);
    const std::vector<LineCmd>& lines = f.context.draw_list().channel(0).lines;
    for (const LineCmd& line : lines)
    {
        CHECK(line.from[1] == doctest::Approx(line.to[1]));
    }
}

TEST_CASE("GUI plot: NaN and infinity break the line into gaps")
{
    PlotFixture f;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float data[7] = { 1.0f, 2.0f, 3.0f, nan, 2.0f, std::numeric_limits<float>::infinity(), 4.0f };
    const auto body = [&] { std::ignore = gui::plot_lines("p", values(data), plot_options()); };
    f.run(body);
    CHECK(lines_in(f) == 2);
}

TEST_CASE("GUI plot: a ring buffer and a strided field draw the same line as the array")
{
    PlotFixture f;
    const float array[6] = { 1.0f, 5.0f, 2.0f, 4.0f, 3.0f, 0.0f };
    const float ring[6] = { 2.0f, 4.0f, 3.0f, 0.0f, 1.0f, 5.0f };
    struct Row
    {
        float pad;
        float value;
    };
    const Row rows[6] = { { 9, 1.0f }, { 9, 5.0f }, { 9, 2.0f }, { 9, 4.0f }, { 9, 3.0f }, { 9, 0.0f } };
    Values source = values(array);
    const auto body = [&] { std::ignore = gui::plot_lines("p", source, plot_options()); };
    f.run(body);
    const std::string reference = dump(f.context.draw_list());
    source = values_ring(ring, 6, 4);
    f.run(body);
    CHECK(dump(f.context.draw_list()) == reference);
    source = values_strided(&rows[0].value, 6, sizeof(Row));
    f.run(body);
    CHECK(dump(f.context.draw_list()) == reference);
}

TEST_CASE("GUI plot: every style records its own commands")
{
    PlotFixture f;
    const float data[5] = { 1.0f, 4.0f, 2.0f, 5.0f, 3.0f };
    PlotStyle style = PlotStyle::Line;
    const auto body = [&] { std::ignore = gui::plot_lines("p", values(data), plot_options(), style); };
    f.run(body);
    const size_t base_rects = f.context.draw_list().channel(0).rects.size();
    CHECK(lines_in(f) == 4);
    style = PlotStyle::Step;
    f.run(body);
    CHECK(lines_in(f) == 8);
    style = PlotStyle::Points;
    f.run(body);
    CHECK(lines_in(f) == 0);
    CHECK(f.context.draw_list().channel(0).rects.size() == base_rects + 5);
    style = PlotStyle::Area;
    f.run(body);
    CHECK(lines_in(f) == 4);
    CHECK(f.context.draw_list().channel(0).rects.size() == base_rects + 5);
}

TEST_CASE("GUI plot: a hundred thousand samples cost at most two points per pixel column and keep a lone peak")
{
    PlotFixture f;
    std::vector<float> data(100000, 1.0f);
    data[54321] = 9.0f;
    const auto body = [&] { std::ignore = gui::plot_lines("p", values(data.data(), static_cast<uint32_t>(data.size())), plot_options()); };
    f.run(body);
    CHECK(lines_in(f) <= 2 * 100);
    float lowest = 1.0e9f;
    for (const LineCmd& line : f.context.draw_list().channel(0).lines)
    {
        lowest = math::min(lowest, math::min(line.from[1], line.to[1]));
    }
    PlotFixture reference;
    std::vector<float> flat(100000, 1.0f);
    const auto flat_body = [&] { std::ignore = gui::plot_lines("p", values(flat.data(), static_cast<uint32_t>(flat.size())), plot_options()); };
    reference.run(flat_body);
    float flat_lowest = 1.0e9f;
    for (const LineCmd& line : reference.context.draw_list().channel(0).lines)
    {
        flat_lowest = math::min(flat_lowest, math::min(line.from[1], line.to[1]));
    }
    CHECK(lowest < flat_lowest - 5.0f);
}

TEST_CASE("GUI plot: markers, legend and hover index")
{
    PlotFixture f;
    const float a[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    const float b[10] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };
    PlotResult result;
    float marker_x = 0.0f;
    const auto body = [&]
    {
        PlotOptions options = plot_options();
        options.width = fixed(160.0f);
        options.legend = true;
        gui::PlotScope plot("p", options);
        plot.line("alpha", values(a));
        plot.line("beta", values(b), PlotStyle::Step);
        plot.vline(4.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
        plot.hline(5.0f, { 1.0f, 0.0f, 0.0f, 1.0f });
        marker_x = to_screen(plot.area(), 4.5f, 0.0f)[0];
        result = plot.result();
    };
    f.run(body);
    CHECK(f.context.draw_list().channel(0).texts.size() >= 2);
    bool vertical = false;
    for (const LineCmd& line : f.context.draw_list().channel(0).lines)
    {
        vertical = vertical || (line.from[0] == doctest::Approx(marker_x) && line.to[0] == doctest::Approx(marker_x));
    }
    CHECK(vertical);
    CHECK(result.hover_index == gui::k_no_index);
    Rect rect;
    REQUIRE(f.context.layout_rect(f.context.id("p"), rect));
    f.driver.move_to({ rect.min[0] + rect.size[0] * 0.55f, rect.min[1] + 10.0f });
    f.driver.frame(f.frame_of(body));
    CHECK(result.hover_index == 5);
}

TEST_CASE("GUI plot: the legend, the range labels and the data never overlap, and series and markers stay in the data rectangle")
{
    PlotFixture f;
    f.driver.input().surface_size = { 300.0f, 160.0f };
    const std::vector<float> a = ramp(40);
    PlotArea data;
    const auto body = [&]
    {
        PlotOptions options;
        options.width = fixed(200.0f);
        options.height = fixed(100.0f);
        gui::PlotScope plot("p", options);
        plot.line("alpha", values(a.data(), 40));
        plot.line("beta", values(a.data(), 40), PlotStyle::Step);
        plot.vline(3.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
        plot.hline(10.0f, { 1.0f, 0.0f, 0.0f, 1.0f });
        data = plot.area();
    };
    f.run(body);
    const DrawList& list = f.context.draw_list();
    const DrawChannel& channel = list.channel(0);

    std::vector<float> legend_x;
    uint32_t gutter_labels = 0;
    for (const TextCmd& text : channel.texts)
    {
        if (text.align == TextAlign::Right)
        {
            CHECK(text.origin[0] <= data.rect.min[0] + 0.01f);
            ++gutter_labels;
        }
        else
        {
            CHECK(text.origin[1] <= data.rect.min[1] + 0.01f);
            legend_x.push_back(text.origin[0]);
        }
    }
    CHECK(gutter_labels == 2);
    REQUIRE(legend_x.size() == 2);
    CHECK(legend_x[0] < legend_x[1]);

    uint32_t clipped_lines = 0;
    for (const LineCmd& line : channel.lines)
    {
        REQUIRE(line.clip != k_no_clip);
        const Rect& clip = list.clip(line.clip);
        CHECK(clip.min[0] >= data.rect.min[0] - 0.01f);
        CHECK(clip.min[1] >= data.rect.min[1] - 0.01f);
        CHECK(clip.min[0] + clip.size[0] <= data.rect.min[0] + data.rect.size[0] + 0.01f);
        CHECK(clip.min[1] + clip.size[1] <= data.rect.min[1] + data.rect.size[1] + 0.01f);
        ++clipped_lines;
    }
    CHECK(clipped_lines > 0);
}

TEST_CASE("GUI plot: a ninth series throws and leaves the frame recoverable")
{
    PlotFixture f;
    const float data[2] = { 1.0f, 2.0f };
    f.driver.frame(f.frame_of([&] { gui::PlotScope plot("warm", plot_options()); plot.line("a", values(data)); }));
    f.context.begin_frame(f.driver.input());
    {
        gui::PlotScope plot("p", plot_options());
        for (uint32_t index = 0; index < k_palette_size; ++index)
        {
            plot.line("s", values(data));
        }
        CHECK_THROWS_AS(plot.line("s", values(data)), Error);
    }
    f.context.abort_frame();
}

TEST_CASE("GUI histogram and sparkline: one bar per value and a bare line")
{
    PlotFixture f;
    const float data[4] = { 1.0f, 0.0f, 3.0f, 2.0f };
    const auto body = [&]
    {
        std::ignore = gui::plot_histogram("h", values(data), plot_options());
        SparklineOptions options;
        std::ignore = gui::sparkline("s", values(data), options);
    };
    f.run(body);
    CHECK(lines_in(f) == 3);
    CHECK(f.context.draw_list().channel(0).texts.empty());
    // Three non-zero bars plus the frame fill.
    CHECK(f.context.draw_list().channel(0).rects.size() + f.context.draw_list().channel(0).rounded_rects.size() >= 4);
}

TEST_CASE("GUI heat: a cell shows its colour and a grid reports hover and click by index")
{
    GuiFixture f;
    const float data[6] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };
    const ColourScale scale = sequential_scale();
    HeatGridResult result;
    ItemState cell;
    const auto body = [&]
    {
        cell = gui::heat_cell("c", 1.0f, scale);
        HeatGridOptions options;
        options.cell = { 10.0f, 10.0f };
        options.gap = 2.0f;
        result = gui::heat_grid("grid", values(data), 3, scale, options);
    };
    f.driver.settle(f.column_of(body));
    Rect rect;
    REQUIRE(f.context.layout_rect(f.context.id("grid"), rect));
    CHECK(rect.size[0] == doctest::Approx(3 * 10.0f + 2 * 2.0f));
    CHECK(rect.size[1] == doctest::Approx(2 * 10.0f + 2.0f));
    f.driver.move_to({ rect.min[0] + 12.0f + 5.0f, rect.min[1] + 12.0f + 5.0f });
    f.driver.frame(f.column_of(body));
    CHECK(result.hovered_index == 4);
    f.driver.move_to({ rect.min[0] + 10.5f, rect.min[1] + 5.0f });
    f.driver.frame(f.column_of(body));
    CHECK(result.hovered_index == gui::k_no_index);
    f.driver.click({ rect.min[0] + 5.0f, rect.min[1] + 17.0f }, f.column_of(body));
    CHECK(result.clicked_index == 3);
    bool top_colour = false;
    for (const RectCmd& command : f.context.draw_list().channel(0).rects)
    {
        top_colour = top_colour || approx_equal(command.colour, scale.high);
    }
    CHECK(top_colour);
}

TEST_CASE("GUI data: warm frames of every data widget allocate nothing")
{
    PlotFixture f;
    std::vector<float> data = ramp(3000);
    const ColourScale scale = diverging_scale();
    const auto body = [&]
    {
        gui::bar("bar", 3.0f, 10.0f);
        {
            gui::PlotScope plot("plot", plot_options());
            plot.line("a", values(data.data(), static_cast<uint32_t>(data.size())), PlotStyle::Area);
            plot.bars("b", values(data.data(), 20));
            plot.vline(3.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
        }
        std::ignore = gui::sparkline("spark", values(data.data(), 50));
        std::ignore = gui::heat_grid("grid", values(data.data(), 9), 3, scale);
        std::ignore = gui::heat_cell("cell", 1.0f, scale, { .show_value = true });
        gui::legend_item("legend", { 1.0f, 0.0f, 0.0f, 1.0f });
    };
    f.driver.run_frames(6, f.column_of(body));
    const MemoryStats before = test::all_allocations();
    f.driver.run_frames(3, f.column_of(body));
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
