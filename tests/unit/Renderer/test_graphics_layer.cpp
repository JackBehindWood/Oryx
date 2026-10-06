#include "doctest.h"

#include "Oryx.h"
#include "NullWindow.h"

using namespace oryx;

namespace
{

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

TEST_CASE("GraphicsLayer takes its size from the window and follows resizes")
{
    GraphicsApp app(true);
    CHECK(app.layer->width() == 320);
    CHECK(app.layer->height() == 200);

    app.window->inject_resize(640, 400);
    CHECK(app.layer->width() == 640);
    CHECK(app.layer->height() == 400);
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
