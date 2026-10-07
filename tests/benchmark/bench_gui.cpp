#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/Renderer/FakeFontSource.h"

#include <chrono>

using namespace oryx;

namespace
{

using Clock = std::chrono::steady_clock;

struct Scene
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    GuiContext context;
    ContextScope<GuiContext> scope{ context };
    ImInput input;

    Scene()
    {
        GuiTheme theme;
        theme.font = &font;
        context.set_theme(theme);
        input.surface_size = { 1280.0f, 720.0f };
        input.delta_time = 1.0f / 60.0f;
        input.pointer.valid = true;
        input.pointer.position = { 300.0f, 200.0f };
    }

    template<typename Body>
    double milliseconds_per_frame(uint32_t frames, Body&& body)
    {
        const auto run = [&]
        {
            context.begin_frame(input);
            LayoutStyle root;
            root.width = grow();
            root.height = grow();
            root.direction = Direction::Column;
            gui::BoxScope box("root", root);
            body();
        };
        for (uint32_t warm = 0; warm < 3; ++warm)
        {
            run();
            context.end_frame();
        }
        const Clock::time_point start = Clock::now();
        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            run();
            context.end_frame();
        }
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count() / static_cast<double>(frames);
    }
};

} // namespace

TEST_SUITE("benchmark")
{

TEST_CASE("Benchmark: GUI frame of 500 widgets")
{
    Scene scene;
    float slider = 0.5f;
    bool flag = false;
    const double ms = scene.milliseconds_per_frame(200, [&]
    {
        for (uint32_t row = 0; row < 100; ++row)
        {
            IdScope id(scene.context, scene.context.index_id(row));
            gui::RowScope line("row");
            gui::label("label");
            std::ignore = gui::button("button");
            std::ignore = gui::checkbox("check", flag);
            std::ignore = gui::slider_float("slider", slider, 0.0f, 1.0f);
            gui::bar("bar", 0.5f, 1.0f);
        }
    });
    MESSAGE("500 widgets: " << ms << " ms/frame");
}

TEST_CASE("Benchmark: GUI frame of a 1000-row virtualised table")
{
    Scene scene;
    const double ms = scene.milliseconds_per_frame(500, [&]
    {
        gui::TableScope table("trace", { .row_count = 1000, .height = fixed(400.0f) });
        table.column("Ply", fixed(60.0f), true);
        table.column("Action", grow());
        table.column("Value", grow());
        std::ignore = table.headers();
        for (uint32_t row = table.first_row(); row < table.last_row(); ++row)
        {
            std::ignore = table.row(row);
            table.cell(scene.context.arena().format("%u", row));
            table.cell(scene.context.arena().format("action %u", row % 9));
            table.cell(scene.context.arena().format("%.3f", static_cast<double>(row) * 0.001));
        }
    });
    MESSAGE("1k-row table: " << ms << " ms/frame");
}

TEST_CASE("Benchmark: GUI frame of four 512-sample plots")
{
    Scene scene;
    std::vector<float> data(512);
    for (uint32_t index = 0; index < 512; ++index)
    {
        data[index] = std::sin(static_cast<float>(index) * 0.05f);
    }
    const double ms = scene.milliseconds_per_frame(500, [&]
    {
        for (uint32_t plot = 0; plot < 4; ++plot)
        {
            IdScope id(scene.context, scene.context.index_id(plot));
            gui::PlotOptions options;
            options.height = fixed(120.0f);
            std::ignore = gui::plot_lines("plot", values(data.data(), 512), options);
        }
    });
    MESSAGE("4 x 512-sample plots: " << ms << " ms/frame");
}

TEST_CASE("Benchmark: GUI plot decimation of 100k samples")
{
    Scene scene;
    std::vector<float> data(100000);
    for (uint32_t index = 0; index < data.size(); ++index)
    {
        data[index] = std::sin(static_cast<float>(index) * 0.001f);
    }
    const double ms = scene.milliseconds_per_frame(50, [&]
    {
        gui::PlotOptions options;
        options.height = fixed(120.0f);
        std::ignore = gui::plot_lines("plot", values(data.data(), static_cast<uint32_t>(data.size())), options);
    });
    MESSAGE("one 100k-sample plot: " << ms << " ms/frame");
}

TEST_CASE("Benchmark: replaying a 500-widget GUI frame into the batcher")
{
    UniquePtr<RendererContext> renderer = create_renderer_context({ RHIBackend::Null });
    std::vector<DrawItem> sink;
    BatchRenderer2D batcher{ batch_renderer_desc(*renderer, sink) };
    Camera2D camera = Camera2D::screen_space(1280.0f, 720.0f);
    Scene scene;
    float slider = 0.5f;
    bool flag = false;
    scene.milliseconds_per_frame(1, [&]
    {
        for (uint32_t row = 0; row < 100; ++row)
        {
            IdScope id(scene.context, scene.context.index_id(row));
            gui::RowScope line("row");
            gui::label("label");
            std::ignore = gui::button("button");
            std::ignore = gui::checkbox("check", flag);
            std::ignore = gui::slider_float("slider", slider, 0.0f, 1.0f);
            gui::bar("bar", 0.5f, 1.0f);
        }
    });
    const uint32_t frames = 200;
    const auto replay_once = [&]
    {
        sink.clear();
        batcher.recycle(0);
        batcher.begin(camera, { sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { 1280.0f, 720.0f } });
        replay(scene.context.draw_list(), batcher, scene.font, { { 0.0f, 0.0f }, { 1280.0f, 720.0f } });
        batcher.end();
    };
    replay_once();
    replay_once();
    const Clock::time_point start = Clock::now();
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        replay_once();
    }
    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count() / static_cast<double>(frames);
    const BatchStats& stats = batcher.stats();
    MESSAGE("500 widgets replay: " << ms << " ms/frame, " << stats.primitives << " prims, " << stats.vertices << " verts, " << stats.bytes << " B, " << stats.draws << " draws");
    renderer->rhi->wait_idle();
}

}
