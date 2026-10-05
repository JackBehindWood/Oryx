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
    int32_t exit_code = play_console_game("0\n3\n1\n4\n2\n", output);

    CHECK(exit_code == 0);
    CHECK(output.find("Player 1 wins!") != std::string::npos);
    CHECK(output.find("wins!") == output.rfind("wins!"));
}

TEST_CASE("BoardLayer closes the application when stdin runs out mid-game, without announcing an outcome")
{
    std::string output;
    int32_t exit_code = play_console_game("0\n3\n", output);

    CHECK(exit_code == 0);
    CHECK(output.find("wins!") == std::string::npos);
    CHECK(output.find("draw") == std::string::npos);
}

TEST_CASE("BoardLayer treats an unterminated last line as a move")
{
    std::string output;
    int32_t exit_code = play_console_game("0\n3\n1\n4\n2", output);

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
