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

TEST_CASE("BoardLayer closes the application on Escape in a window")
{
    WindowedGame game;
    game.window->inject_key(KeyCode::Escape, true);
    game.frame();
    CHECK(game.app.closing());
}

#endif
