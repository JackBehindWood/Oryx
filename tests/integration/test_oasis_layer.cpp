#include "doctest.h"

#include "Oryx.h"

#ifdef OX_ENABLE_GRAPHICS

#include "NullRHI.h"
#include "NullWindow.h"
#include "Oasis/Core/OasisLayer.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

constexpr Vec2f k_window = { 800.0f, 600.0f };

// Points the GUI's layout file at a scratch directory so no test reads or writes the real one.
struct LayoutFile
{
    LayoutFile()
    {
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        update_settings<GuiSettings>([this](GuiSettings& settings) { settings.layout_file = file(); settings.animations = false; });
    }

    ~LayoutFile()
    {
        reset_settings();
        std::filesystem::remove_all(dir);
    }

    [[nodiscard]] std::filesystem::path file() const { return dir / "layout.yaml"; }

    [[nodiscard]] std::string text() const
    {
        std::ifstream in(file());
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    void write(const std::string& content) const { std::ofstream(file()) << content; }

    std::filesystem::path dir = std::filesystem::temp_directory_path() / "oryx_oasis_shell_test";
};

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

bool find_annotation_kind(const BoardAnnotations& annotations, AnnotationKind kind)
{
    return std::any_of(annotations.items.begin(), annotations.items.end(), [kind](const BoardAnnotation& item) { return item.kind == kind; });
}

struct ShellRig
{
    LayoutFile layout_file;

    ShellRig(const std::string& game, const std::string& opponent, const std::string& layout_text = std::string())
        : renderer(RHIBackend::Null)
        , app({ 0, nullptr })
    {
        if (!layout_text.empty())
        {
            layout_file.write(layout_text);
        }
        window = static_cast<NullWindow*>(&app.adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", static_cast<int32_t>(k_window[0]), static_cast<int32_t>(k_window[1]) })));
        graphics = &app.push_overlay<GraphicsLayer>();
        simulation = &app.push_layer<SimulationLayer>();
        shell = create_unique<oasis::OasisLayer>(oasis::Options{});
        shell->use_graphics(graphics->router());
        graphics->add_client(*shell, { FramePhase::Interface });
        BoardFactory factory = [this](const std::string& name, PlayerId seat) -> SharedPtr<IBoard>
        {
            SharedPtr<IGraphicsBoard> created = create_graphics_board(name, seat);
            board = dynamic_cast<PresentedGraphicsBoard2D*>(created.get());
            graphics->add_client(*created);
            shell->board_created(*created, name);
            return created;
        };
        BoardReleaser release = [this](IBoard& released)
        {
            if (shell != nullptr)
            {
                shell->board_released(dynamic_cast<IGraphicsBoard&>(released));
            }
            graphics->remove_client(dynamic_cast<IGraphicsBoard&>(released));
        };
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

    [[nodiscard]] bool board_has_text(std::string_view text) const
    {
        const LayoutTree& tree = board->ui().layout();
        for (uint32_t index = 0; index < tree.node_count(); ++index)
        {
            if (tree.node(index).paint.text == text)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] Vec2f board_centre_of(std::string_view text) const
    {
        const LayoutTree& tree = board->ui().layout();
        for (uint32_t index = 0; index < tree.node_count(); ++index)
        {
            if (tree.node(index).paint.text == text)
            {
                return board_region().min + rect_centre(tree.node(index).rect);
            }
        }
        FAIL("no board box with text " << text);
        return {};
    }

    void play_until_decisions(size_t count)
    {
        for (uint32_t attempt = 0; attempt < 40 && !layer->match_in_progress(); ++attempt)
        {
            frames(5);
            if (!layer->match_in_progress())
            {
                click_at(cell(4));
            }
        }
        frames(20);
        for (SpaceId space = 0; space < 9 && shell->feed().size() < count; ++space)
        {
            click_at(cell(space));
            frames(10);
        }
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

    void close_dashboard()
    {
        shell->set_dashboard_open(false);
        frames(3);
    }

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

    [[nodiscard]] bool panel_open(std::string_view name) const { return gui::is_open(shell->layout(), gui::make_panel_id(name)); }

    RendererScope renderer;
    Application app;
    NullWindow* window = nullptr;
    GraphicsLayer* graphics = nullptr;
    SimulationLayer* simulation = nullptr;
    BoardLayer* layer = nullptr;
    PresentedGraphicsBoard2D* board = nullptr;
    UniquePtr<oasis::OasisLayer> shell;
    WorldProbe probe;
};

} // namespace

TEST_CASE("OasisLayer hands the board the viewport panel's rect, which the views beside it narrow")
{
    {
        ShellRig rig("hexapawn", selection::k_human_opponent);
        rig.frames(3);
        CHECK(rig.board_region().size[0] < k_window[0]);
        CHECK(rig.board_region().size[0] > 0.0f);
        CHECK(rig.board_region().min[1] > 0.0f);
        const float board_bottom = rig.board_region().min[1] + rig.board_region().size[1];
        CHECK(board_bottom < k_window[1]);
        CHECK(board_bottom > 0.5f * k_window[1]);
        CHECK(rig.probe.size[0] == doctest::Approx(rig.board_region().size[0]));
    }
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.close_dashboard();
    rig.frames(3);
    CHECK(rig.board_region().size[0] == doctest::Approx(k_window[0]));
}

TEST_CASE("OasisLayer: the first run opens the dashboard and a saved closed layout stays closed")
{
    std::string saved;
    {
        ShellRig rig("hexapawn", selection::k_human_opponent);
        rig.frames(3);
        CHECK(rig.shell->dashboard_open());
        rig.close_dashboard();
        rig.frames(60);
        CHECK_FALSE(rig.shell->dashboard_open());
        saved = rig.layout_file.text();
    }
    REQUIRE_FALSE(saved.empty());
    ShellRig again("hexapawn", selection::k_human_opponent, saved);
    again.frames(3);
    CHECK_FALSE(again.shell->dashboard_open());
}

TEST_CASE("OasisLayer: a click on the panel never reaches the board, a click on the board does")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
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

TEST_CASE("OasisLayer: an open menu claims Escape, which closes it; the next Escape reaches the board")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
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

TEST_CASE("OasisLayer: the Window menu hides and shows the dashboard and the board follows")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.frames(3);
    const float with_panel = rig.board_region().size[0];

    rig.click_text("Window");
    rig.frames(2);
    rig.click_text("Dashboard");
    rig.frames(3);
    CHECK_FALSE(rig.shell->dashboard_open());
    CHECK(rig.board_region().size[0] == doctest::Approx(k_window[0]));

    rig.click_text("Window");
    rig.frames(2);
    rig.click_text("Dashboard");
    rig.frames(3);
    CHECK(rig.shell->dashboard_open());
    CHECK(rig.board_region().size[0] == doctest::Approx(with_panel));
}

TEST_CASE("OasisLayer: Help opens the GUI showcase as a docked panel beside the views")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.frames(3);
    const float with_panel = rig.board_region().size[0];
    CHECK_FALSE(rig.shell->showcase_open());

    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(3);
    CHECK(rig.shell->showcase_open());
    CHECK(rig.panel_open("tools/gui-showcase"));
    CHECK(rig.board_region().size[0] == doctest::Approx(with_panel));

    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(3);
    CHECK_FALSE(rig.shell->showcase_open());
}

TEST_CASE("OasisLayer: Reset layout restores the default and Undo layout takes back the last change")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.frames(3);
    const gui::DockLayout initial = rig.shell->layout();

    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(4);
    REQUIRE(rig.shell->showcase_open());

    rig.click_text("Window");
    rig.frames(2);
    rig.click_text("Undo layout");
    rig.frames(4);
    CHECK_FALSE(rig.shell->showcase_open());
    CHECK(gui::equal(rig.shell->layout(), initial));

    rig.click_text("Window");
    rig.frames(2);
    rig.click_text("Redo layout");
    rig.frames(4);
    CHECK(rig.shell->showcase_open());

    rig.click_text("Window");
    rig.frames(2);
    rig.click_text("Reset layout");
    rig.frames(4);
    CHECK_FALSE(rig.shell->showcase_open());
    CHECK(gui::equal(rig.shell->layout(), initial));
}

TEST_CASE("OasisLayer: the Panels menu lists the dock panels and reopens a closed one")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.frames(3);
    rig.click_text("Window");
    rig.frames(2);
    rig.click_text("Panels  >");
    rig.frames(2);
    REQUIRE(rig.has_text("GUI Showcase"));
    rig.click_text("GUI Showcase");
    rig.frames(3);
    CHECK(rig.panel_open("tools/gui-showcase"));
}

TEST_CASE("OasisLayer: autosave writes the layout after a change and an untouched run writes nothing")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.frames(60);
    CHECK_FALSE(std::filesystem::exists(rig.layout_file.file()));
    CHECK(rig.shell->dashboard_open());

    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(60);
    CHECK(std::filesystem::exists(rig.layout_file.file()));
    CHECK(rig.layout_file.text().find("gui-showcase") != std::string::npos);
}

TEST_CASE("OasisLayer: the dashboard's annotation flags are kept in the layout file and read back")
{
    std::string saved;
    {
        ShellRig rig("tictactoe", "minimax");
        rig.frames(3);
        const AnnotationFlags before = rig.shell->annotation_flags();
        rig.click_text("View");
        rig.frames(2);
        rig.click_text("Move values");
        rig.frames(60);
        CHECK(rig.shell->annotation_flags().value != before.value);
        saved = rig.layout_file.text();
        CHECK(saved.find("workbench.dashboard") != std::string::npos);
    }
    ShellRig again("tictactoe", "minimax", saved);
    again.frames(6);
    CHECK(again.shell->annotation_flags().value);
}

TEST_CASE("OasisLayer: the status bar shows the decision counts and its Console button tabs the console beside Trace")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(4);
    const LayoutTree& tree = rig.shell->context().layout();
    bool counts = false;
    for (uint32_t index = 0; index < tree.node_count(); ++index)
    {
        counts = counts || tree.node(index).paint.text.rfind("Match ", 0) == 0;
    }
    CHECK(counts);
    CHECK_FALSE(rig.shell->console_open());
    CHECK_FALSE(rig.panel_open("console/log"));

    rig.click_text("Console");
    rig.frames(3);
    CHECK(rig.panel_open("console/log"));
    CHECK_FALSE(rig.shell->console_open());
    CHECK(rig.has_text("Follow"));

    rig.click_text("Console");
    rig.frames(3);
    CHECK_FALSE(rig.panel_open("console/log"));
}

TEST_CASE("OasisLayer: without a Trace panel the Console button opens and closes the drawer")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(4);
    {
        ContextScope<GuiContext> scope(const_cast<GuiContext&>(rig.shell->context()));
        CHECK(gui::set_panel_open("dashboard/trace", false).applied);
    }
    rig.frames(3);
    CHECK_FALSE(rig.has_text("search"));

    rig.click_text("Console");
    rig.frames(3);
    CHECK(rig.shell->console_open());
    CHECK(rig.has_text("search"));
    CHECK(rig.has_text("Follow"));

    rig.click_text("Console");
    rig.frames(3);
    CHECK_FALSE(rig.shell->console_open());
    CHECK_FALSE(rig.has_text("Follow"));
}

TEST_CASE("OasisLayer: the Window menu lists the workbenches and the console panel opens from it")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.frames(3);
    rig.click_text("Window");
    rig.frames(2);
    REQUIRE(rig.has_text("Dashboard"));
    REQUIRE(rig.has_text("Console"));
    rig.click_text("Console");
    rig.frames(4);
    CHECK(rig.panel_open("console/log"));
}

TEST_CASE("OasisLayer: a layout file from a newer build is never overwritten")
{
    const std::string newer = "version: 99\nsurfaces: []\n";
    ShellRig rig("hexapawn", selection::k_human_opponent, newer);
    rig.frames(3);
    rig.click_text("Help");
    rig.frames(2);
    rig.click_text("GUI Showcase");
    rig.frames(60);
    CHECK(rig.shell->showcase_open());
    CHECK(rig.layout_file.text() == newer);
}

TEST_CASE("OasisLayer: a corrupt layout file is set aside and the default layout is used")
{
    ShellRig rig("hexapawn", selection::k_human_opponent, "version: 1\nsurfaces: nope\n");
    rig.frames(3);
    CHECK(std::filesystem::exists(std::filesystem::path(rig.layout_file.file()) += ".bad"));
    CHECK(rig.panel_open("viewport/0"));
}

TEST_CASE("OasisLayer: switching asks first once a move was played, and Cancel keeps the match")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
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

TEST_CASE("OasisLayer: confirming a switch starts the new match with a new board and an empty feed")
{
    ShellRig rig("tictactoe", "minimax");
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
    for (SpaceId space = 0; space < 9 && rig.shell->feed().size() < 2; ++space)
    {
        rig.click_at(rig.cell(space));
        rig.frames(10);
    }
    const size_t before = rig.shell->feed().size();
    REQUIRE(before > 1);
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

TEST_CASE("OasisLayer: decisions use the board's own names and the board shows its annotation controls")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(3);
    rig.play_until_decisions(1);
    REQUIRE(rig.shell->feed().size() >= 1);

    BoardScene scene;
    rig.board->presentation().build_scene(k_no_space, scene);
    const RecordView first = rig.shell->feed().view(0);
    REQUIRE(first.record->chosen_space_count == 1);
    CHECK(label_view(first.record->chosen_label) == scene.layout->label(first.record->chosen_spaces[0]));
    CHECK(rig.has_text("Heat"));
    CHECK(rig.has_text("Chosen"));
    CHECK(rig.has_text("Values"));
    CHECK(rig.shell->annotation_flags() == AnnotationFlags{ true, true, false });
    CHECK(find_annotation_kind(rig.shell->annotations(), AnnotationKind::Chosen));
}

TEST_CASE("OasisLayer: the board toolbar and the View menu switch the same annotation flags")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(4);
    CHECK_FALSE(rig.shell->annotation_flags().value);
    rig.click_text("Values");
    rig.frames(3);
    CHECK(rig.shell->annotation_flags().value);
    CHECK(rig.board->annotation_flags().value);

    rig.click_text("View");
    rig.frames(2);
    rig.click_text("Board heat");
    rig.frames(3);
    CHECK_FALSE(rig.shell->annotation_flags().heat);
    CHECK_FALSE(rig.board->annotation_flags().heat);
}

TEST_CASE("OasisLayer: an older selected decision is shown as a ghost and following removes the badge")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(3);
    rig.play_until_decisions(2);
    REQUIRE(rig.shell->feed().size() >= 2);
    CHECK(rig.shell->model().follow_latest);
    CHECK_FALSE(rig.board_has_text("Ghost"));

    select_record(rig.shell->model(), rig.shell->feed().view(0).record->sequence);
    rig.frames(3);
    CHECK(rig.board_has_text("Ghost"));

    follow_latest_record(rig.shell->model());
    rig.frames(3);
    CHECK_FALSE(rig.board_has_text("Ghost"));
}

TEST_CASE("OasisLayer: pointing at a board space tells the Decision row which action ends there")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(3);
    rig.play_until_decisions(1);
    REQUIRE(rig.shell->feed().size() >= 1);
    const RecordView latest = rig.shell->feed().view(rig.shell->feed().size() - 1);
    REQUIRE(latest.record->chosen_space_count == 1);

    rig.window->inject_cursor(rig.cell(latest.record->chosen_spaces[0])[0], rig.cell(latest.record->chosen_spaces[0])[1]);
    rig.frames(4);
    CHECK(rig.shell->model().board_hovered_action == latest.record->chosen);

    rig.window->inject_cursor(1.0f, 1.0f);
    rig.frames(4);
    CHECK(rig.shell->model().board_hovered_action == INVALID_ACTION);
}

TEST_CASE("OasisLayer: a new match leaves nothing of the previous one on the board")
{
    ShellRig rig("tictactoe", "minimax");
    rig.frames(3);
    rig.play_until_decisions(2);
    REQUIRE(rig.shell->feed().size() >= 2);
    select_record(rig.shell->model(), rig.shell->feed().view(0).record->sequence);
    rig.shell->model().pinned_action = rig.shell->feed().view(0).record->chosen;
    rig.frames(3);
    REQUIRE(rig.board_has_text("Ghost"));

    rig.shell->feed().begin_match();
    rig.frames(3);
    CHECK_FALSE(rig.board_has_text("Ghost"));
    CHECK(rig.shell->model().follow_latest);
    CHECK(rig.shell->model().pinned_action == INVALID_ACTION);
    CHECK_FALSE(find_annotation_kind(rig.shell->annotations(), AnnotationKind::Chosen));
    CHECK_FALSE(find_annotation_kind(rig.shell->annotations(), AnnotationKind::Heat));
}

TEST_CASE("OasisLayer: the annotation controls vanish with the dashboard")
{
    ShellRig rig("tictactoe", "minimax");
    rig.close_dashboard();
    rig.frames(4);
    CHECK_FALSE(rig.board_has_text("Heat"));
}

TEST_CASE("OasisLayer: a switch before any move happens at once")
{
    ShellRig rig("hexapawn", selection::k_human_opponent);
    rig.close_dashboard();
    rig.frames(3);
    rig.click_text("Game");
    rig.frames(2);
    rig.click_text("tictactoe");
    rig.frames(4);
    CHECK_FALSE(rig.shell->switch_pending());
    CHECK(rig.layer->game_name() == "tictactoe");
}

TEST_CASE("OasisLayer: the opponent menu changes only the opponent")
{
    ShellRig rig("tictactoe", selection::k_human_opponent);
    rig.close_dashboard();
    rig.frames(3);
    rig.click_text("Opponent");
    rig.frames(2);
    rig.click_text("random");
    rig.frames(4);
    CHECK(rig.layer->game_name() == "tictactoe");
    CHECK(rig.layer->opponent_name() == "random");
}

TEST_CASE("OasisLayer: the chrome lists the menus and a warm frame allocates nothing")
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    LayoutFile layout_file;
    PolledInput input;
    InputRouter router;
    oasis::OasisLayer shell{ oasis::Options{} };
    shell.use_graphics(router);
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
    for (const char* name : { "Game", "Opponent", "View", "Window", "Help" })
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
