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
    app.window->inject_close();
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
