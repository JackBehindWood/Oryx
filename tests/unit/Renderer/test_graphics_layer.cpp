#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "NullWindow.h"

using namespace oryx;

namespace
{

struct RendererScope
{
    RendererScope() { Renderer::init({ RHIBackend::Null }); }
    ~RendererScope() { Renderer::shutdown(); }
};

struct GraphicsSettingsScope
{
    GraphicsSettings saved = settings_of<GraphicsSettings>();

    ~GraphicsSettingsScope()
    {
        update_settings<GraphicsSettings>([this](GraphicsSettings& settings) { settings = saved; });
    }
};

void edit_graphics(const std::function<void(GraphicsSettings&)>& edit)
{
    update_settings<GraphicsSettings>(edit);
}

class GraphicsApp : public Application
{
public:
    explicit GraphicsApp(bool with_window) : Application({})
    {
        if (with_window)
        {
            window = static_cast<NullWindow*>(&adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", 320, 200 })));
        }
        layer = &push_overlay<GraphicsLayer>();
    }

    int32_t disabled = 0;
    NullWindow* window = nullptr;
    GraphicsLayer* layer = nullptr;

protected:
    void on_layer_disabled(Layer&, std::string_view) override
    {
        ++disabled;
        close(1);
    }
};

} // namespace

TEST_CASE("GraphicsLayer without a window is disabled")
{
    GraphicsApp app(false);
    CHECK(app.disabled == 1);
    CHECK(app.layer->is_disabled());
    CHECK(app.exit_code() == 1);
}

TEST_CASE("GraphicsLayer's viewport takes its size from the window and follows resizes")
{
    RendererScope renderer;
    GraphicsApp app(true);
    REQUIRE(app.layer->viewport());
    CHECK(app.layer->viewport()->width() == 320);
    CHECK(app.layer->viewport()->height() == 200);

    app.window->inject_resize(640, 400);
    CHECK(app.layer->viewport()->width() == 640);
    CHECK(app.layer->viewport()->height() == 400);
}

TEST_CASE("GraphicsLayer closes the application on window close")
{
    GraphicsApp app(true);
    CHECK_FALSE(app.closing());
    app.window->inject_close();
    CHECK(app.closing());
    CHECK(app.exit_code() == 0);
    CHECK(app.disabled == 0);
}

TEST_CASE("GraphicsLayer update pumps the window")
{
    GraphicsApp app(true);
    app.window->inject_key(KeyCode::A, true);
    CHECK(app.window->input().key_pressed(KeyCode::A));
    app.layer->update(0.016);
    CHECK_FALSE(app.window->input().key_pressed(KeyCode::A));
    CHECK(app.window->input().key_down(KeyCode::A));
}

namespace
{

class CountingClient : public IFrameClient
{
public:
    void frame(const FrameInfo& info) override
    {
        ++frames;
        last_logical = info.logical;
        last_framebuffer = info.framebuffer;
        last_scale = info.scale;
        last_delta = info.delta_time;
        input = &info.input;
        if (fail)
        {
            throw Error("client failed");
        }
    }

    int32_t frames = 0;
    Vec2f last_logical;
    Vec2f last_framebuffer;
    float last_scale = 0.0f;
    double last_delta = 0.0;
    const IInput* input = nullptr;
    bool fail = false;
};

} // namespace

TEST_CASE("GraphicsLayer calls each frame client once per frame with the window's sizes and input")
{
    GraphicsApp app(true);
    CountingClient first;
    CountingClient second;
    app.layer->add_client(first);
    app.layer->add_client(second);

    app.layer->update(0.5);
    CHECK(first.frames == 1);
    CHECK(second.frames == 1);
    CHECK(first.last_logical == Vec2f(320.0f, 200.0f));
    CHECK(first.last_framebuffer == Vec2f(320.0f, 200.0f));
    CHECK(first.last_scale == 1.0f);
    CHECK(first.last_delta == 0.5);
    CHECK(first.input == &app.window->input());

    app.window->inject_scale(2.0f);
    app.layer->update(0.25);
    CHECK(first.last_logical == Vec2f(320.0f, 200.0f));
    CHECK(first.last_framebuffer == Vec2f(640.0f, 400.0f));
    CHECK(first.last_scale == 2.0f);

    app.layer->remove_client(first);
    app.layer->update(0.25);
    CHECK(first.frames == 2);
    CHECK(second.frames == 3);
}

TEST_CASE("GraphicsLayer drops a frame client that throws and keeps pumping the window")
{
    GraphicsApp app(true);
    CountingClient bad;
    CountingClient good;
    bad.fail = true;
    app.layer->add_client(bad);
    app.layer->add_client(good);

    app.layer->update(0.016);
    app.layer->update(0.016);
    CHECK(bad.frames == 1);
    CHECK(good.frames == 2);
    CHECK_FALSE(app.layer->is_disabled());
}

TEST_CASE("GraphicsLayer applies vsync from the settings at attach and when they change")
{
    GraphicsSettingsScope scope;
    RendererScope renderer;
    edit_graphics([](GraphicsSettings& settings) { settings.vsync = false; });
    GraphicsApp app(true);
    REQUIRE(app.layer->viewport());
    NullViewport& viewport = static_cast<NullViewport&>(*app.layer->viewport());
    CHECK_FALSE(viewport.vsync());

    edit_graphics([](GraphicsSettings& settings) { settings.vsync = true; });
    CHECK(viewport.vsync());
    edit_graphics([](GraphicsSettings& settings) { settings.vsync = false; });
    CHECK_FALSE(viewport.vsync());
}

TEST_CASE("GraphicsLayer stops following the settings once detached")
{
    GraphicsSettingsScope scope;
    RendererScope renderer;
    {
        GraphicsApp app(true);
    }
    CHECK_NOTHROW(edit_graphics([](GraphicsSettings& settings) { settings.vsync = !settings.vsync; }));
}

TEST_CASE("GraphicsLayer sleeps idle_sleep_ms when the frame was not presented")
{
    GraphicsSettingsScope scope;
    RendererScope renderer;
    GraphicsApp app(true);
    edit_graphics([](GraphicsSettings& settings) { settings.idle_sleep_ms = 40; });

    using clock = std::chrono::steady_clock;
    clock::time_point start = clock::now();
    app.layer->update(0.016);
    CHECK(clock::now() - start < std::chrono::milliseconds(40));

    app.window->inject_resize(0, 0);
    start = clock::now();
    app.layer->update(0.016);
    CHECK(clock::now() - start >= std::chrono::milliseconds(40));

    edit_graphics([](GraphicsSettings& settings) { settings.idle_sleep_ms = 0; });
    start = clock::now();
    app.layer->update(0.016);
    CHECK(clock::now() - start < std::chrono::milliseconds(40));
}

TEST_CASE("GraphicsLayer holds frames to max_fps and leaves them uncapped at 0")
{
    GraphicsSettingsScope scope;
    RendererScope renderer;
    GraphicsApp app(true);
    edit_graphics([](GraphicsSettings& settings) { settings.max_fps = 50; });

    using clock = std::chrono::steady_clock;
    app.layer->update(0.016);
    const clock::time_point start = clock::now();
    for (int32_t i = 0; i < 5; ++i)
    {
        app.layer->update(0.016);
    }
    CHECK(clock::now() - start >= std::chrono::milliseconds(95));

    edit_graphics([](GraphicsSettings& settings) { settings.max_fps = 0; });
    const clock::time_point uncapped = clock::now();
    for (int32_t i = 0; i < 5; ++i)
    {
        app.layer->update(0.016);
    }
    CHECK(clock::now() - uncapped < std::chrono::milliseconds(80));
}

TEST_CASE("GraphicsLayer reloads shaders on the reload key and keeps running when the reload fails")
{
    GraphicsSettingsScope scope;
    RendererScope renderer;
    GraphicsApp app(true);
    const GraphicsPipelineDesc quad = pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format());

    GraphicsPipelineHandle before = Renderer::pipeline(quad);
    app.layer->update(0.016);
    CHECK_NOTHROW((void)Renderer::resolve_pipeline(before));

    app.window->inject_key(KeyCode::F5, true);
    app.layer->update(0.016);
    CHECK_THROWS_AS((void)Renderer::resolve_pipeline(before), Error);
    CHECK_FALSE(app.layer->is_disabled());

    GraphicsPipelineHandle after = Renderer::pipeline(pipeline_desc(pipeline_def(Primitive2D::Quad), Renderer::shaders(), Renderer::back_buffer_format()));
    app.window->inject_key(KeyCode::F5, false);
    app.layer->update(0.016);
    edit_graphics([](GraphicsSettings& settings) { settings.reload_key = ""; });
    app.window->inject_key(KeyCode::F5, true);
    app.layer->update(0.016);
    CHECK_NOTHROW((void)Renderer::resolve_pipeline(after));
}

TEST_CASE("GraphicsLayer is disabled and shuts down cleanly when the device fails to end a frame")
{
    {
        RendererScope renderer;
        GraphicsApp app(true);
        static_cast<NullRHI&>(Renderer::rhi()).fail_next_end_frame();
        app.run();
        CHECK(app.disabled == 1);
        CHECK(app.layer->is_disabled());
        CHECK(app.exit_code() == 1);
    }
    CHECK(RHIResource::live_count() == 0);
}

namespace
{

class SceneClient : public IFrameClient, public RenderSource
{
public:
    void frame(const FrameInfo&) override
    {
        scene_open = Renderer::scene().open();
        Renderer::scene().submit(*this);
    }

    void render_stage(RenderStage stage, StageContext& context) override
    {
        if (stage == RenderStage::Scene2D)
        {
            ++drawn;
            logical = context.view.logical;
            if (fail)
            {
                throw Error("stage failed");
            }
        }
    }

    bool scene_open = false;
    bool fail = false;
    int32_t drawn = 0;
    Vec2f logical;
};

} // namespace

TEST_CASE("GraphicsLayer opens one scene around its clients and ends it before the frame is recorded")
{
    RendererScope renderer;
    GraphicsApp app(true);
    SceneClient client;
    app.layer->add_client(client);

    app.layer->update(0.016);
    CHECK(client.scene_open);
    CHECK(client.drawn == 1);
    CHECK(client.logical == Vec2f(320.0f, 200.0f));
    CHECK_FALSE(Renderer::scene().open());
}

TEST_CASE("GraphicsLayer drops its clients when a scene stage fails and keeps running")
{
    RendererScope renderer;
    GraphicsApp app(true);
    SceneClient client;
    client.fail = true;
    app.layer->add_client(client);

    CHECK_NOTHROW(app.layer->update(0.016));
    CHECK(client.drawn == 1);
    CHECK_FALSE(Renderer::scene().open());
    CHECK(app.disabled == 0);

    CHECK_NOTHROW(app.layer->update(0.016));
    CHECK(client.drawn == 1);
}

TEST_CASE("GraphicsLayer keeps a scene valid for a zero-sized window")
{
    RendererScope renderer;
    GraphicsApp app(true);
    SceneClient client;
    app.layer->add_client(client);
    app.window->inject_resize(0, 0);
    CHECK_NOTHROW(app.layer->update(0.016));
    CHECK_FALSE(Renderer::scene().open());
}
