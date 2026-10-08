#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

int32_t play_console_game(const std::string& input, std::string& out_output)
{
    ConsoleScope console(input);
    Application app({ 0, nullptr });
    app.push_layer<SimulationLayer>();
    app.push_layer<BoardLayer>(BoardLayerDesc{ "tictactoe", selection::k_human_opponent });
    app.run();
    out_output = console.output();
    return app.exit_code();
}

} // namespace

TEST_CASE("BoardLayer plays a console game to its end, announces the outcome once and closes")
{
    std::string output;
    int32_t exit_code = play_console_game("a3\na2\nb3\nb2\nc3\n", output);

    CHECK(exit_code == 0);
    CHECK(output.find("Player 1 wins!") != std::string::npos);
    CHECK(output.find("wins!") == output.rfind("wins!"));
    CHECK(output.find("X wins") != std::string::npos);
}

TEST_CASE("BoardLayer closes the application when stdin runs out mid-game, without announcing an outcome")
{
    std::string output;
    int32_t exit_code = play_console_game("a3\na2\n", output);

    CHECK(exit_code == 0);
    CHECK(output.find("wins!") == std::string::npos);
    CHECK(output.find("draw") == std::string::npos);
}

TEST_CASE("BoardLayer treats an unterminated last line as a move")
{
    std::string output;
    int32_t exit_code = play_console_game("a3\na2\nb3\nb2\nc3", output);

    CHECK(exit_code == 0);
    CHECK(output.find("Player 1 wins!") != std::string::npos);
}

TEST_CASE("BoardLayer fails with an error for an unknown game")
{
    ConsoleScope console("");
    Application app({ 0, nullptr });
    app.push_layer<SimulationLayer>();
    app.push_layer<BoardLayer>(BoardLayerDesc{ "no-such-game", "" });
    app.run();

    CHECK(app.exit_code() == 1);
}

namespace
{

// A windowless board layer whose boards are fakes, so a test sees which boards were made and released.
struct SwitchRig
{
    explicit SwitchRig(const std::string& game, const std::string& opponent)
        : app({ 0, nullptr })
    {
        simulation = &app.push_layer<SimulationLayer>();
        BoardFactory factory = [this](const std::string& name, PlayerId) -> SharedPtr<IBoard>
        {
            created.push_back(name);
            boards.push_back(create_shared<FakeBoard>());
            return boards.back();
        };
        BoardReleaser releaser = [this](IBoard& board) { released.push_back(&board); };
        layer = &app.push_layer<BoardLayer>(BoardLayerDesc{ game, opponent, factory, false, releaser });
    }

    void frame()
    {
        simulation->update(0.016);
        layer->update(0.016);
    }

    void request(const std::string& game, const std::string& opponent)
    {
        StartMatchEvent event(game, opponent);
        app.post_event(event);
    }

    std::vector<std::string> created;
    std::vector<SharedPtr<FakeBoard>> boards;
    std::vector<IBoard*> released;
    // Last, so the layers detach (and call the releaser) before the vectors above go.
    Application app;
    SimulationLayer* simulation = nullptr;
    BoardLayer* layer = nullptr;
};

} // namespace

TEST_CASE("A StartMatchEvent is only recorded when posted and applied at the layer's next update")
{
    SwitchRig rig("tictactoe", "minimax");
    rig.frame();
    REQUIRE(rig.created.size() == 1);

    rig.request("hexapawn", "");
    CHECK(rig.created.size() == 1);
    CHECK(rig.released.empty());

    rig.frame();
    REQUIRE(rig.created.size() == 2);
    CHECK(rig.created.back() == "hexapawn");
    REQUIRE(rig.released.size() == 1);
    CHECK(rig.released.front() == rig.boards.front().get());
    CHECK_FALSE(rig.layer->is_disabled());
}

TEST_CASE("Changing the opponent alone keeps the game, and a game change drops an opponent that does not fit it")
{
    SwitchRig rig("tictactoe", "tictactoe/heuristic");
    rig.frame();

    rig.request("", "random");
    rig.frame();
    REQUIRE(rig.created.size() == 2);
    CHECK(rig.created.back() == "tictactoe");

    rig.request("", "tictactoe/heuristic");
    rig.frame();
    rig.request("hexapawn", "");
    rig.frame();
    REQUIRE(rig.created.size() == 4);
    CHECK(rig.created.back() == "hexapawn");
}

TEST_CASE("A StartMatchEvent naming an unknown game or opponent is rejected and the current match carries on")
{
    SwitchRig rig("tictactoe", "minimax");
    rig.frame();

    rig.request("no-such-game", "");
    rig.frame();
    rig.request("", "no-such-strategy");
    rig.frame();
    rig.request("", "hexapawn/heuristic");
    rig.frame();

    CHECK(rig.created.size() == 1);
    CHECK(rig.released.empty());
    CHECK_FALSE(rig.layer->is_disabled());
    CHECK_FALSE(rig.app.closing());

    rig.request("", "random");
    rig.frame();
    CHECK(rig.created.size() == 2);
}

TEST_CASE("BoardLayer names its match and reports progress only after a move in a game that is not over")
{
    SwitchRig rig("tictactoe", selection::k_human_opponent);
    CHECK(rig.layer->game_name() == "tictactoe");
    CHECK(rig.layer->opponent_name() == selection::k_human_opponent);
    rig.frame();
    CHECK_FALSE(rig.layer->match_in_progress());

    rig.boards.front()->next_action = 0;
    rig.frame();
    rig.boards.front()->next_action = PENDING_ACTION;
    CHECK(rig.layer->match_in_progress());

    rig.request("hexapawn", "");
    rig.frame();
    CHECK(rig.layer->game_name() == "hexapawn");
    CHECK_FALSE(rig.layer->match_in_progress());
}

TEST_CASE("BoardLayer releases its board when it is detached")
{
    SwitchRig rig("tictactoe", "minimax");
    rig.frame();
    rig.layer->detach();
    REQUIRE(rig.released.size() == 1);
    CHECK(rig.released.front() == rig.boards.front().get());
}

#ifdef OX_ENABLE_GRAPHICS

#include "NullWindow.h"

namespace
{

constexpr Vec2f k_window = { 800.0f, 600.0f };

// Declared before the Application so the device outlives the layers that hold RHI resources.
struct RendererScope
{
    explicit RendererScope(RHIBackend backend) { Renderer::init({ backend }); }
    ~RendererScope() { Renderer::shutdown(); }
};

struct WindowedGame
{
    WindowedGame()
        : renderer(RHIBackend::Null)
        , app({ 0, nullptr })
    {
        window = static_cast<NullWindow*>(&app.adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", static_cast<int32_t>(k_window[0]), static_cast<int32_t>(k_window[1]) })));
        graphics = &app.push_overlay<GraphicsLayer>();
        simulation = &app.push_layer<SimulationLayer>();
        BoardFactory factory = [this](const std::string& game, PlayerId seat) -> SharedPtr<IBoard>
        {
            SharedPtr<IGraphicsBoard> created = create_graphics_board(game, seat);
            board = dynamic_cast<PresentedGraphicsBoard2D*>(created.get());
            graphics->add_client(*created);
            return created;
        };
        layer = &app.push_layer<BoardLayer>(BoardLayerDesc{ "hexapawn", selection::k_human_opponent, factory, false });
    }


    void frame()
    {
        simulation->update(0.016);
        layer->update(0.016);
        graphics->update(0.016);
    }

    void click(SpaceId space)
    {
        BoardScene scene;
        board->presentation().build_scene(k_no_space, scene);
        BoardProjection2D layout = fit_board_2d(scene, k_window);
        Vec2f world = board_to_world(layout, scene.layout->position(space));
        window->inject_cursor(world[0], k_window[1] - world[1]);
        window->inject_mouse_button(MouseCode::Left, true);
        frame();
        window->inject_mouse_button(MouseCode::Left, false);
        frame();
    }

    RendererScope renderer;
    Application app;
    NullWindow* window = nullptr;
    SimulationLayer* simulation = nullptr;
    BoardLayer* layer = nullptr;
    GraphicsLayer* graphics = nullptr;
    PresentedGraphicsBoard2D* board = nullptr;
};

} // namespace

TEST_CASE("BoardLayer plays a presented game in a window from clicks alone, then restarts it")
{
    WindowedGame game;
    REQUIRE(game.board != nullptr);
    game.frame();
    CHECK(game.board->presentation().view().status == "White to move");

    for (SpaceId space : { 7, 4, 0, 3, 4, 2 })
    {
        game.click(space);
    }
    game.frame();
    CHECK(game.board->presentation().terminal());
    CHECK(game.board->presentation().view().status == "White wins");

    for (int32_t frame = 0; frame < 30; ++frame)
    {
        game.frame();
    }
    game.window->inject_key(KeyCode::R, true);
    for (int32_t frame = 0; frame < 4; ++frame)
    {
        game.frame();
    }
    CHECK_FALSE(game.board->presentation().terminal());
    CHECK(game.board->presentation().view().status == "White to move");
}

TEST_CASE("Switching games in a window removes the old board from the GraphicsLayer")
{
    RendererScope renderer(RHIBackend::Null);
    Application app({ 0, nullptr });
    app.adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", 800, 600 }));
    GraphicsLayer& graphics = app.push_overlay<GraphicsLayer>();
    SimulationLayer& simulation = app.push_layer<SimulationLayer>();
    BoardFactory factory = [&graphics](const std::string& game, PlayerId seat) -> SharedPtr<IBoard>
    {
        SharedPtr<IGraphicsBoard> board = create_graphics_board(game, seat);
        graphics.add_client(*board);
        return board;
    };
    BoardReleaser releaser = [&graphics](IBoard& board) { graphics.remove_client(dynamic_cast<IGraphicsBoard&>(board)); };
    BoardLayer& layer = app.push_layer<BoardLayer>(BoardLayerDesc{ "tictactoe", selection::k_human_opponent, factory, false, releaser });
    CHECK(graphics.client_count() == 1);

    StartMatchEvent request("hexapawn", "");
    app.post_event(request);
    CHECK(graphics.client_count() == 1);
    simulation.update(0.016);
    layer.update(0.016);
    graphics.update(0.016);
    CHECK(graphics.client_count() == 1);
    CHECK_FALSE(app.closing());
}

TEST_CASE("BoardLayer closes the application on Escape in a window")
{
    WindowedGame game;
    game.window->inject_key(KeyCode::Escape, true);
    game.frame();
    CHECK(game.app.closing());
}

#endif
