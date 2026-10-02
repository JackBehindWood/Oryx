#include "doctest.h"

#include "Oryx.h"
#include "NullWindow.h"

using namespace oryx;

namespace
{

class RecordingLayer : public Layer
{
public:
    RecordingLayer() : Layer("Recording") {}

    void event(Event& event) override { types.push_back(event.event_type()); }

    std::vector<EventType> types;
};

class WindowApp : public Application
{
public:
    WindowApp() : Application({})
    {
        window = static_cast<NullWindow*>(&adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", 640, 480 })));
        recorder = &push_layer<RecordingLayer>();
    }

    NullWindow* window = nullptr;
    RecordingLayer* recorder = nullptr;
};

} // namespace

TEST_CASE("Application owns no window until asked")
{
    Application app({});
    CHECK(app.window() == nullptr);
}

TEST_CASE("NullWindow reports its description as a headless native handle")
{
    NullWindow window({ "Headless", 800, 600 });
    NativeWindowHandle handle = window.native_handle();
    CHECK(handle.native == nullptr);
    CHECK(handle.width == 800);
    CHECK(handle.framebuffer_height == 600);
    CHECK(handle.content_scale == 1.0f);
    CHECK_FALSE(window.should_close());
}

#if !(defined(OX_ENABLE_GRAPHICS) && defined(OX_PLATFORM_MACOS))
TEST_CASE("Window::create falls back to NullWindow without a platform backend")
{
    UniquePtr<Window> window = Window::create({});
    CHECK(dynamic_cast<NullWindow*>(window.get()) != nullptr);
}
#endif

TEST_CASE("NullWindow injection updates input and posts events in order")
{
    WindowApp app;
    app.window->inject_key(KeyCode::A, true);
    app.window->inject_cursor(10.0f, 20.0f);
    app.window->inject_mouse_button(MouseCode::Left, true);
    app.window->inject_mouse_button(MouseCode::Left, false);
    app.window->inject_scroll(0.0f, 1.0f);
    app.window->inject_key(KeyCode::A, false);

    std::vector<EventType> expected = {
        EventType::KeyPressed, EventType::MouseMoved, EventType::MouseButtonPressed,
        EventType::MouseButtonReleased, EventType::MouseScrolled, EventType::KeyReleased
    };
    CHECK(app.recorder->types == expected);

    const IInput& input = app.window->input();
    Vec2f cursor;
    input.cursor_position(cursor);
    CHECK(cursor[0] == 10.0f);
    CHECK(input.key_released(KeyCode::A));
    CHECK(input.mouse_released(MouseCode::Left));

    app.window->poll_events();
    CHECK_FALSE(input.key_released(KeyCode::A));
}

TEST_CASE("NullWindow resize, focus and close")
{
    WindowApp app;
    app.window->inject_resize(1024, 768);
    CHECK(app.window->native_handle().width == 1024);
    CHECK(app.window->native_handle().framebuffer_height == 768);

    app.window->inject_focus(false);
    CHECK_FALSE(app.window->should_close());
    app.window->inject_close();
    CHECK(app.window->should_close());

    std::vector<EventType> expected = { EventType::WindowResize, EventType::WindowFocus, EventType::WindowClose };
    CHECK(app.recorder->types == expected);
}

TEST_CASE("request_close marks the window without posting an event")
{
    WindowApp app;
    app.window->request_close();
    CHECK(app.window->should_close());
    CHECK(app.recorder->types.empty());
}

TEST_CASE("window outlives the layers that use it during detach")
{
    struct DetachReader : Layer
    {
        explicit DetachReader(bool* alive) : Layer("DetachReader"), seen(alive) {}
        void attach() override { window = Application::Get().window(); }
        void detach() override { *seen = !window->should_close(); }
        Window* window = nullptr;
        bool* seen;
    };

    bool saw_window = false;
    {
        Application app({});
        app.adopt_window(create_unique<NullWindow>(WindowDesc{}));
        app.push_layer<DetachReader>(&saw_window);
    }
    CHECK(saw_window);
}

TEST_CASE("static Input reads the primary window and is empty without one")
{
    {
        Application app({});
        Vec2f cursor(5.0f, 5.0f);
        CHECK_FALSE(Input::key_down(KeyCode::A));
        Input::cursor_position(cursor);
        CHECK(cursor[0] == 0.0f);
    }

    WindowApp app;
    app.window->inject_key(KeyCode::A, true);
    app.window->inject_cursor(7.0f, 9.0f);
    app.window->inject_mouse_button(MouseCode::Left, true);
    CHECK(Input::key_down(KeyCode::A));
    CHECK(Input::key_pressed(KeyCode::A));
    CHECK(Input::mouse_down(MouseCode::Left));
    CHECK_FALSE(Input::key_released(KeyCode::A));
    Vec2f cursor;
    Input::cursor_position(cursor);
    CHECK(cursor[1] == 9.0f);
}
