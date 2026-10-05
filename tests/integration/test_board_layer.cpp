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
    app.push_layer<BoardLayer>(BoardLayerDesc{ selection::FrontEnd::Console, "tictactoe", selection::kHumanOpponent });
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
    app.push_layer<BoardLayer>(BoardLayerDesc{ selection::FrontEnd::Console, "no-such-game", "" });
    app.run();

    CHECK(app.exit_code() == 1);
}

#ifdef OX_ENABLE_GRAPHICS

#include "NullWindow.h"

namespace
{

constexpr Vec2f kWindow = { 800.0f, 600.0f };

struct WindowedGame
{
    WindowedGame()
        : app({ 0, nullptr })
    {
        Renderer::init({ RHIBackend::Null });
        window = static_cast<NullWindow*>(&app.adopt_window(create_unique<NullWindow>(WindowDesc{ "Test", static_cast<int32_t>(kWindow[0]), static_cast<int32_t>(kWindow[1]) })));
        simulation = &app.push_layer<SimulationLayer>();
        GraphicsBoardFactory factory = [this](const std::string& game, PlayerId seat)
        {
            UniquePtr<IGraphicsBoard> created = create_graphics_board(game, seat);
            board = dynamic_cast<PresentedGraphicsBoard*>(created.get());
            return created;
        };
        layer = &app.push_layer<BoardLayer>(BoardLayerDesc{ selection::FrontEnd::Graphical, "hexapawn", selection::kHumanOpponent, factory });
    }

    ~WindowedGame() { Renderer::shutdown(); }

    void frame()
    {
        simulation->update(0.016);
        layer->update(0.016);
        window->poll_events();
    }

    void click(SpaceId space)
    {
        BoardScene scene;
        board->presentation().build_scene(kNoSpace, scene);
        BoardLayout2D layout = fit_board_2d(scene, kWindow);
        const Vec3f& position = scene.spaces[space].space.position;
        Vec2f world = board_to_world(layout, { position[0], position[1] });
        window->inject_cursor(world[0], kWindow[1] - world[1]);
        window->inject_mouse_button(MouseCode::Left, true);
        frame();
        window->inject_mouse_button(MouseCode::Left, false);
        frame();
    }

    Application app;
    NullWindow* window = nullptr;
    SimulationLayer* simulation = nullptr;
    BoardLayer* layer = nullptr;
    PresentedGraphicsBoard* board = nullptr;
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
    game.frame();
    game.frame();
    game.frame();
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
