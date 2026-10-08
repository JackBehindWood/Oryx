#include "doctest.h"

#include "Oryx.h"

#ifdef OX_ENABLE_GRAPHICS

#include "NullRHI.h"
#include "NullWindow.h"
#include "Oasis/Core/OasisShell.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

constexpr Vec2f k_window = { 800.0f, 600.0f };

// Declared before the Application so the device outlives the layers that hold RHI resources.
struct RendererScope
{
    explicit RendererScope(RHIBackend backend) { Renderer::init({ backend }); }
    ~RendererScope() { Renderer::shutdown(); }
};

// A world client that records what the router lets through to the board's view.
class WorldProbe final : public IFrameClient
{
public:
    void frame(const FrameInfo& info) override
    {
        pressed = pressed || info.input.mouse_pressed(MouseCode::Left);
        escape = escape || info.input.key_pressed(KeyCode::Escape);
        size = info.logical;
    }

    void clear()
    {
        pressed = false;
        escape = false;
    }

    bool pressed = false;
    bool escape = false;
    Vec2f size{ 0.0f, 0.0f };
};

struct ShellRig
{
    ShellRig(oasis::DashboardFlag flag, const std::string& game, const std::string& opponent)
        : renderer(RHIBackend::Null)
        , app({ 0, nullptr })
    {
        window = static_cast<NullWindow*>(&app.adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", static_cast<int32_t>(k_window[0]), static_cast<int32_t>(k_window[1]) })));
        graphics = &app.push_overlay<GraphicsLayer>();
        simulation = &app.push_layer<SimulationLayer>();
        shell = create_unique<oasis::OasisShell>(graphics->router(), flag);
        graphics->add_client(*shell, { FramePhase::Interface });
        BoardFactory factory = [this](const std::string& name, PlayerId seat) -> SharedPtr<IBoard>
        {
            SharedPtr<IGraphicsBoard> created = create_graphics_board(name, seat);
            board = dynamic_cast<PresentedGraphicsBoard2D*>(created.get());
            graphics->add_client(*created);
            return created;
        };
        BoardReleaser release = [this](IBoard& released) { graphics->remove_client(dynamic_cast<IGraphicsBoard&>(released)); };
        layer = &app.push_layer<BoardLayer>(BoardLayerDesc{ game, opponent, factory, false, release, &shell->feed() });
        shell->bind(*layer);
        graphics->add_client(probe);
    }

    ~ShellRig() { graphics->remove_client(probe); }

    void frame()
    {
        simulation->update(0.016);
        layer->update(0.016);
        graphics->update(0.016);
    }

    void frames(uint32_t count)
    {
        for (uint32_t index = 0; index < count; ++index)
        {
            frame();
        }
    }

    void click_at(const Vec2f& at)
    {
        window->inject_cursor(at[0], at[1]);
        frame();
        window->inject_mouse_button(MouseCode::Left, true);
        frame();
        window->inject_mouse_button(MouseCode::Left, false);
        frame();
    }

    [[nodiscard]] Vec2f centre_of(std::string_view text) const
    {
        const LayoutTree& tree = shell->context().layout();
        for (uint32_t index = 0; index < tree.node_count(); ++index)
        {
            if (tree.node(index).paint.text == text)
            {
                return rect_centre(tree.node(index).rect);
            }
        }
        FAIL("no box with text " << text);
        return {};
    }

    [[nodiscard]] bool has_text(std::string_view text) const
    {
        const LayoutTree& tree = shell->context().layout();
        for (uint32_t index = 0; index < tree.node_count(); ++index)
        {
            if (tree.node(index).paint.text == text)
            {
                return true;
            }
        }
        return false;
    }

    void click_text(std::string_view text) { click_at(centre_of(text)); }

    [[nodiscard]] Vec2f cell(SpaceId space) const
    {
        BoardScene scene;
        board->presentation().build_scene(k_no_space, scene);
        const ViewRegion& region = graphics->router().view(k_main_view);
        const BoardProjection2D layout = fit_board_2d(scene, region.size);
        const Vec2f world = board_to_world(layout, scene.layout->position(space));
        return { region.min[0] + world[0], region.min[1] + region.size[1] - world[1] };
    }

    [[nodiscard]] const ViewRegion& board_region() const { return graphics->router().view(k_main_view); }

    RendererScope renderer;
    Application app;
    NullWindow* window = nullptr;
    GraphicsLayer* graphics = nullptr;
    SimulationLayer* simulation = nullptr;
    BoardLayer* layer = nullptr;
    PresentedGraphicsBoard2D* board = nullptr;
    UniquePtr<oasis::OasisShell> shell;
    WorldProbe probe;
};

} // namespace

TEST_CASE("OasisShell hands the board the area its side panel leaves")
{
    {
        ShellRig rig(oasis::DashboardFlag::On, "hexapawn", selection::k_human_opponent);
        rig.frames(3);
        const float panel = static_cast<float>(settings_of<DashboardSettings>().panel_width);
        CHECK(rig.board_region().size[0] == doctest::Approx(k_window[0] - panel));
        CHECK(rig.board_region().min[1] > 0.0f);
        CHECK(rig.board_region().min[1] + rig.board_region().size[1] == doctest::Approx(k_window[1]));
        CHECK(rig.probe.size[0] == doctest::Approx(k_window[0] - panel));
    }
    ShellRig rig(oasis::DashboardFlag::Off, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    CHECK(rig.board_region().size[0] == doctest::Approx(k_window[0]));
}

TEST_CASE("OasisShell starts with the dashboard the flag or the setting asks for")
{
    InputRouter router;
    CHECK_FALSE(oasis::OasisShell(router, oasis::DashboardFlag::Default).dashboard_open());
    CHECK(oasis::OasisShell(router, oasis::DashboardFlag::On).dashboard_open());
    CHECK_FALSE(oasis::OasisShell(router, oasis::DashboardFlag::Off).dashboard_open());

    update_settings<DashboardSettings>([](DashboardSettings& settings) { settings.enabled = true; });
    CHECK(oasis::OasisShell(router, oasis::DashboardFlag::Default).dashboard_open());
    CHECK(oasis::OasisShell(router, oasis::DashboardFlag::On).dashboard_open());
    CHECK_FALSE(oasis::OasisShell(router, oasis::DashboardFlag::Off).dashboard_open());
    reset_settings();
}

TEST_CASE("OasisShell: a click on the panel never reaches the board, a click on the board does")
{
    ShellRig rig(oasis::DashboardFlag::On, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    CHECK(rig.board->presentation().view().status == "White to move");

    rig.probe.clear();
    rig.click_at({ k_window[0] - 100.0f, 300.0f });
    CHECK_FALSE(rig.probe.pressed);
    CHECK(rig.board->presentation().view().status == "White to move");

    rig.probe.clear();
    rig.click_at(rig.cell(7));
    CHECK(rig.probe.pressed);
    rig.click_at(rig.cell(4));
    rig.frames(2);
    CHECK(rig.board->presentation().view().status == "Black to move");
}

TEST_CASE("OasisShell: an open menu claims Escape, which closes it; the next Escape reaches the board")
{
    ShellRig rig(oasis::DashboardFlag::On, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    rig.click_text("Game");
    rig.frames(2);
    REQUIRE(rig.shell->context().popup_open());

    rig.probe.clear();
    rig.window->inject_key(KeyCode::Escape, true);
    rig.frame();
    rig.window->inject_key(KeyCode::Escape, false);
    CHECK_FALSE(rig.probe.escape);
    CHECK_FALSE(rig.app.closing());
    rig.frames(2);
    CHECK_FALSE(rig.shell->context().popup_open());

    rig.window->inject_key(KeyCode::Escape, true);
    rig.frame();
    CHECK(rig.app.closing());
}

TEST_CASE("OasisShell: the View menu hides and shows the dashboard and the board follows")
{
    ShellRig rig(oasis::DashboardFlag::On, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    const float with_panel = rig.board_region().size[0];

    rig.click_text("View");
    rig.frames(2);
    rig.click_text("Dashboard");
    rig.frames(3);
    CHECK_FALSE(rig.shell->dashboard_open());
    CHECK(rig.board_region().size[0] == doctest::Approx(k_window[0]));

    rig.click_text("View");
    rig.frames(2);
    rig.click_text("Dashboard");
    rig.frames(3);
    CHECK(rig.shell->dashboard_open());
    CHECK(rig.board_region().size[0] == doctest::Approx(with_panel));
}

TEST_CASE("OasisShell: Help opens the GUI showcase in the dashboard's place")
{
    ShellRig rig(oasis::DashboardFlag::On, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    const float with_panel = rig.board_region().size[0];

    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(3);
    CHECK(rig.shell->showcase_open());
    CHECK(rig.board_region().size[0] == doctest::Approx(with_panel));

    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(3);
    CHECK_FALSE(rig.shell->showcase_open());
}

TEST_CASE("OasisShell: switching asks first once a move was played, and Cancel keeps the match")
{
    ShellRig rig(oasis::DashboardFlag::On, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    rig.click_at(rig.cell(7));
    rig.click_at(rig.cell(4));
    rig.frames(2);
    REQUIRE(rig.layer->match_in_progress());

    rig.click_text("Game");
    rig.frames(2);
    rig.click_text("tictactoe");
    rig.frames(3);
    CHECK(rig.shell->switch_pending());
    CHECK(rig.layer->game_name() == "hexapawn");
    REQUIRE(rig.has_text("Cancel"));

    rig.click_text("Cancel");
    rig.frames(3);
    CHECK_FALSE(rig.shell->switch_pending());
    CHECK(rig.layer->game_name() == "hexapawn");
    CHECK(rig.layer->match_in_progress());
}

TEST_CASE("OasisShell: confirming a switch starts the new match with a new board and an empty feed")
{
    ShellRig rig(oasis::DashboardFlag::On, "tictactoe", "minimax");
    for (uint32_t attempt = 0; attempt < 40 && !rig.layer->match_in_progress(); ++attempt)
    {
        rig.frames(5);
        if (!rig.layer->match_in_progress())
        {
            rig.click_at(rig.cell(4));
        }
    }
    REQUIRE(rig.layer->match_in_progress());
    rig.frames(20);
    const size_t before = rig.shell->feed().size();
    REQUIRE(before > 0);
    const size_t clients = rig.graphics->client_count();

    rig.click_text("Game");
    rig.frames(2);
    rig.click_text("hexapawn");
    rig.frames(3);
    REQUIRE(rig.shell->switch_pending());
    rig.click_text("Switch");
    rig.frames(5);

    CHECK_FALSE(rig.shell->switch_pending());
    CHECK(rig.layer->game_name() == "hexapawn");
    CHECK(rig.graphics->client_count() == clients);
    for (size_t index = 0; index < rig.shell->feed().size(); ++index)
    {
        CHECK(rig.shell->feed().view(index).record->match_index == 0);
    }
    CHECK(rig.shell->feed().size() < before);
}

TEST_CASE("OasisShell: a switch before any move happens at once")
{
    ShellRig rig(oasis::DashboardFlag::Off, "hexapawn", selection::k_human_opponent);
    rig.frames(3);
    rig.click_text("Game");
    rig.frames(2);
    rig.click_text("tictactoe");
    rig.frames(4);
    CHECK_FALSE(rig.shell->switch_pending());
    CHECK(rig.layer->game_name() == "tictactoe");
}

TEST_CASE("OasisShell: the opponent menu changes only the opponent")
{
    ShellRig rig(oasis::DashboardFlag::Off, "tictactoe", selection::k_human_opponent);
    rig.frames(3);
    rig.click_text("Opponent");
    rig.frames(2);
    rig.click_text("random");
    rig.frames(4);
    CHECK(rig.layer->game_name() == "tictactoe");
    CHECK(rig.layer->opponent_name() == "random");
}

TEST_CASE("OasisShell: the chrome lists the menus and a warm frame allocates nothing")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    PolledInput input;
    InputRouter router;
    oasis::OasisShell shell(router, oasis::DashboardFlag::On);
    shell.set_font(&font);
    ImInput im;
    im.surface_size = k_window;
    im.delta_time = 1.0f / 60.0f;
    const auto frame = [&]
    {
        input.begin_frame();
        router.begin_frame(input, k_window);
        shell.run(im);
        router.begin_world();
    };
    for (uint32_t index = 0; index < 4; ++index)
    {
        frame();
    }
    const LayoutTree& tree = shell.context().layout();
    for (const char* name : { "Game", "Opponent", "View", "Help" })
    {
        bool found = false;
        for (uint32_t index = 0; index < tree.node_count(); ++index)
        {
            found = found || tree.node(index).paint.text == name;
        }
        CHECK_MESSAGE(found, name);
    }
    const MemoryStats before = test::all_allocations();
    frame();
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

#endif
