#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "NullWindow.h"
#include "unit/Renderer/RenderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct RendererScope
{
    RendererScope() { Renderer::init({ RHIBackend::Null }); }
    ~RendererScope() { Renderer::shutdown(); }
};

class ViewApp : public Application
{
public:
    ViewApp()
        : Application({})
    {
        window = static_cast<NullWindow*>(&adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", 320, 200 })));
        layer = &push_overlay<GraphicsLayer>();
    }

    NullWindow* window = nullptr;
    GraphicsLayer* layer = nullptr;
};

const ViewRegion k_region{ { 100.0f, 20.0f }, { 220.0f, 180.0f } };

// Logs its frame and, when a scene is open, its Scene2D draw, and keeps what it was handed.
class LoggingClient final : public IFrameClient, public RenderSource
{
public:
    LoggingClient(std::string name, std::vector<std::string>& log)
        : m_name(std::move(name))
        , m_log(log)
    {
    }

    void frame(const FrameInfo& info) override
    {
        m_log.push_back(m_name + ":frame");
        logical = info.logical;
        framebuffer = info.framebuffer;
        scale = info.scale;
        board = read_board_input(info.input, info.logical, info.scale);
        if (fail)
        {
            throw Error("client failed");
        }
        if (Renderer::initialised() && Renderer::scene().open())
        {
            Renderer::scene().submit(*this);
        }
    }

    void render_stage(RenderStage stage, StageContext& context) override
    {
        if (stage == RenderStage::Scene2D)
        {
            m_log.push_back(m_name + ":draw");
            view_logical = context.view.logical;
        }
    }

    Vec2f logical;
    Vec2f framebuffer;
    Vec2f view_logical;
    float scale = 0.0f;
    BoardInput board;
    bool fail = false;

private:
    std::string m_name;
    std::vector<std::string>& m_log;
};

// An interface client that places the main view to the right of a fixed 100 point panel and optionally claims the pointer.
class PanelClient final : public IFrameClient
{
public:
    PanelClient(GraphicsLayer& layer, std::vector<std::string>& log)
        : m_layer(layer)
        , m_log(log)
    {
    }

    void frame(const FrameInfo& info) override
    {
        m_log.push_back("ui:frame");
        if (fail)
        {
            throw Error("panel failed");
        }
        m_layer.router().set_view(k_main_view, { { 100.0f, 20.0f }, { info.logical[0] - 100.0f, info.logical[1] - 20.0f } });
        if (claim)
        {
            m_layer.router().claim_pointer();
        }
    }

    bool claim = false;
    bool fail = false;

private:
    GraphicsLayer& m_layer;
    std::vector<std::string>& m_log;
};

const ClientDesc k_interface{ FramePhase::Interface, k_main_view };

} // namespace

TEST_CASE("GraphicsLayer runs interface clients before world clients whatever the order added")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    PanelClient panel(*app.layer, log);
    app.layer->add_client(world);
    app.layer->add_client(panel, k_interface);

    app.layer->update(0.016);
    const std::vector<std::string> expected = { "ui:frame", "world:frame" };
    CHECK(log == expected);
}

TEST_CASE("GraphicsLayer gives a world client its view's size and a region-local cursor")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    PanelClient panel(*app.layer, log);
    app.layer->add_client(world);
    app.layer->add_client(panel, k_interface);

    app.window->inject_cursor(150.0f, 60.0f);
    app.layer->update(0.016);
    CHECK(world.logical == Vec2f(220.0f, 180.0f));
    CHECK(world.framebuffer == Vec2f(220.0f, 180.0f));
    CHECK(world.board.viewport == Vec2f(220.0f, 180.0f));
    CHECK(world.board.cursor == Vec2f(50.0f, 40.0f));
}

TEST_CASE("GraphicsLayer: a world view's clicks and drags are in region coordinates and stay with the view")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    PanelClient panel(*app.layer, log);
    app.layer->add_client(world);
    app.layer->add_client(panel, k_interface);

    app.window->inject_cursor(150.0f, 60.0f);
    app.window->inject_mouse_button(MouseCode::Left, true);
    app.layer->update(0.016);
    CHECK(world.board.select);
    CHECK(world.board.cursor == Vec2f(50.0f, 40.0f));

    app.window->inject_cursor(10.0f, 10.0f);
    app.layer->update(0.016);
    CHECK(world.board.select_down);
    CHECK(world.board.cursor == Vec2f(-90.0f, -10.0f));

    app.window->inject_mouse_button(MouseCode::Left, false);
    app.layer->update(0.016);
    CHECK(world.board.select_released);
    CHECK_FALSE(world.board.select_down);
}

TEST_CASE("GraphicsLayer: a press that begins on the panel never reaches the world, even dragged over it")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    PanelClient panel(*app.layer, log);
    app.layer->add_client(world);
    app.layer->add_client(panel, k_interface);

    app.window->inject_cursor(50.0f, 60.0f);
    app.window->inject_mouse_button(MouseCode::Left, true);
    app.layer->update(0.016);
    CHECK_FALSE(world.board.select);

    app.window->inject_cursor(200.0f, 100.0f);
    app.layer->update(0.016);
    CHECK_FALSE(world.board.select_down);
    app.window->inject_mouse_button(MouseCode::Left, false);
    app.layer->update(0.016);
    CHECK_FALSE(world.board.select_released);

    panel.claim = true;
    app.window->inject_mouse_button(MouseCode::Left, true);
    app.layer->update(0.016);
    CHECK_FALSE(world.board.select);
}

TEST_CASE("GraphicsLayer: HiDPI scales the region's framebuffer and a resize resizes the view")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    PanelClient panel(*app.layer, log);
    app.layer->add_client(world);
    app.layer->add_client(panel, k_interface);

    app.window->inject_scale(2.0f);
    app.layer->update(0.016);
    CHECK(world.logical == Vec2f(220.0f, 180.0f));
    CHECK(world.framebuffer == Vec2f(440.0f, 360.0f));
    CHECK(world.scale == 2.0f);

    app.window->inject_resize(640, 400);
    app.layer->update(0.016);
    CHECK(world.logical == Vec2f(540.0f, 380.0f));
}

TEST_CASE("GraphicsLayer's default main view is the whole surface and the window's own sizes")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    app.layer->add_client(world);

    app.window->inject_scale(1.5f);
    app.window->inject_cursor(150.0f, 60.0f);
    app.layer->update(0.016);
    NativeWindowHandle handle = app.window->native_handle();
    CHECK(world.logical == Vec2f(static_cast<float>(handle.width), static_cast<float>(handle.height)));
    CHECK(world.framebuffer == Vec2f(static_cast<float>(handle.framebuffer_width), static_cast<float>(handle.framebuffer_height)));
    CHECK(world.board.cursor == Vec2f(150.0f, 60.0f));
}

TEST_CASE("GraphicsLayer drops a throwing client in either phase without skipping the next")
{
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient bad_world("bad_world", log);
    LoggingClient good_world("good_world", log);
    PanelClient bad_panel(*app.layer, log);
    LoggingClient good_panel("good_panel", log);
    bad_world.fail = true;
    bad_panel.fail = true;
    app.layer->add_client(bad_world);
    app.layer->add_client(good_world);
    app.layer->add_client(bad_panel, k_interface);
    app.layer->add_client(good_panel, k_interface);

    app.layer->update(0.016);
    const std::vector<std::string> first = { "ui:frame", "good_panel:frame", "bad_world:frame", "good_world:frame" };
    CHECK(log == first);

    log.clear();
    app.layer->update(0.016);
    const std::vector<std::string> second = { "good_panel:frame", "good_world:frame" };
    CHECK(log == second);
}

TEST_CASE("GraphicsLayer draws the interface above the world and gives each source its client's view")
{
    RendererScope renderer;
    ViewApp app;
    std::vector<std::string> log;
    LoggingClient world("world", log);
    LoggingClient overlay("overlay", log);
    PanelClient panel(*app.layer, log);
    app.layer->add_client(panel, k_interface);
    app.layer->add_client(overlay, k_interface);
    app.layer->add_client(world);

    app.layer->update(0.016);
    const std::vector<std::string> expected = { "ui:frame", "overlay:frame", "world:frame", "world:draw", "overlay:draw" };
    CHECK(log == expected);
    CHECK(world.view_logical == Vec2f(220.0f, 180.0f));
    CHECK(overlay.view_logical == Vec2f(320.0f, 200.0f));
}
