#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "Oasis/Core/Options.h"
#include "Oasis/Game/TicTacToeBoardModel.h"
#include "Oasis/Graphics/TicTacToeLayout.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

TicTacToeState play(std::initializer_list<ActionId> moves)
{
    TicTacToeState state;
    for (ActionId move : moves)
    {
        state.apply(move);
    }
    return state;
}

} // namespace

TEST_CASE("TicTacToeBoardModel describes a running game")
{
    TicTacToeBoardModel model;
    model.update(play({ 4 }));

    CHECK(model.mark_at(1, 1) == Mark::X);
    CHECK(model.current_player() == 1);
    CHECK_FALSE(model.terminal());
    CHECK(model.winner() == -1);
    CHECK(model.status_text() == "O to move");
    CHECK_FALSE(model.legal(1, 1));
    CHECK(model.legal(0, 0));
    CHECK_FALSE(model.legal(3, 0));
    CHECK(TicTacToeBoardModel::action_for(2, 1) == 7);
}

TEST_CASE("TicTacToeBoardModel reports a win and a draw")
{
    TicTacToeBoardModel model;
    model.update(play({ 0, 3, 1, 4, 2 }));
    CHECK(model.terminal());
    CHECK(model.winner() == 0);
    CHECK(model.status_text() == "X wins");

    model.update(play({ 0, 1, 2, 4, 3, 5, 7, 6, 8 }));
    CHECK(model.terminal());
    CHECK(model.winner() == -1);
    CHECK(model.status_text() == "Draw");
}

TEST_CASE("TicTacToeLayout maps a cursor to the cell under it")
{
    TicTacToeLayout layout = TicTacToeLayout::fit({ 800.0f, 600.0f });
    size_t row = 9;
    size_t col = 9;

    Vec2f centre = layout.cell_centre(0, 2);
    CHECK(layout.cell_at(centre, row, col));
    CHECK(row == 0);
    CHECK(col == 2);

    CHECK(layout.cell_at(layout.cell_centre(2, 0), row, col));
    CHECK(row == 2);
    CHECK(col == 0);

    CHECK_FALSE(layout.cell_at({ 1.0f, 1.0f }, row, col));
}

TEST_CASE("TicTacToeLayout hit-testing finds every cell at any window size when window size and cursor share units")
{
    for (Vec2f viewport : { Vec2f(800.0f, 600.0f), Vec2f(1600.0f, 1200.0f), Vec2f(500.0f, 900.0f) })
    {
        TicTacToeLayout layout = TicTacToeLayout::fit(viewport);
        for (size_t row = 0; row < 3; ++row)
        {
            for (size_t col = 0; col < 3; ++col)
            {
                Vec2f cursor = layout.world_from_cursor(layout.cell_centre(row, col));
                size_t hit_row = 9;
                size_t hit_col = 9;
                CHECK(layout.cell_at(layout.world_from_cursor(cursor), hit_row, hit_col));
                CHECK(hit_row == row);
                CHECK(hit_col == col);
            }
        }
    }
}

TEST_CASE("TicTacToeLayout flips the cursor's y so the top of the window is the top row")
{
    TicTacToeLayout layout = TicTacToeLayout::fit({ 800.0f, 600.0f });
    Vec2f top_cursor = { 400.0f, 10.0f };
    CHECK(layout.world_from_cursor(top_cursor)[1] == doctest::Approx(590.0f));
}

TEST_CASE("plan_launch lets --simulate win, then picks the front-end once")
{
    Options options;
    LaunchPlan plan;

    options.simulate = "random,random,1";
    CHECK(plan_launch(options, true, plan));
    CHECK(plan.mode == LaunchMode::Simulate);

    options.simulate.clear();
    options.headless = true;
    CHECK(plan_launch(options, true, plan));
    CHECK(plan.mode == LaunchMode::Console);

    options.headless = false;
    CHECK(plan_launch(options, false, plan));
    CHECK(plan.mode == LaunchMode::Console);

    CHECK(plan_launch(options, true, plan));
    CHECK(plan.mode == LaunchMode::Console);

    GraphicsBoardRegistry::register_factory("tictactoe", [](const Params&) -> UniquePtr<IGraphicsBoard> { return create_unique<FakeGraphicsBoard>(); });
    CHECK(plan_launch(options, true, plan));
    CHECK(plan.mode == LaunchMode::Graphical);
    CHECK(plan.game == "tictactoe");
    GraphicsBoardRegistry::unregister_factory("tictactoe");

    options.game = "no-such-game";
    CHECK_FALSE(plan_launch(options, true, plan));
}

TEST_CASE("read_options takes Oasis' own and the board options from parsed arguments")
{
    oryx::ParsedArgs parsed = oryx::CommandLine::global().parse(std::vector<std::string>{ "--console", "--game=nim", "--opponent=human", "--simulate=a,b,2", "--benchmark", "--rhi=null" });
    Options options = read_options(parsed);

    CHECK(options.headless);
    CHECK(options.benchmark);
    CHECK(options.game == "nim");
    CHECK(options.opponent == "human");
    CHECK(options.simulate == "a,b,2");
    CHECK(options.rhi == "null");
    CHECK_THROWS_AS(oryx::CommandLine::global().parse(std::vector<std::string>{ "--unknown" }), oryx::Error);
}
