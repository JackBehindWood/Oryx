#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "Oasis/Core/Options.h"
#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"

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

BoardView describe(const IBoardPresenter& presenter, const IState& state)
{
    BoardView view;
    presenter.describe(state, 0, view);
    return view;
}

} // namespace

TEST_CASE("TicTacToePresenter shows each mark on its cell and names the side to move")
{
    TicTacToePresenter presenter;
    BoardView view = describe(presenter, play({ 4, 0 }));

    REQUIRE(view.spaces.size() == 9);
    CHECK(view.spaces[0].label == "a3");
    CHECK(view.spaces[8].label == "c1");
    CHECK(view.pieces == std::vector<BoardPiece>{ { TicTacToePresenter::kMark, 1, 0 }, { TicTacToePresenter::kMark, 0, 4 } });
    CHECK(view.status == "X to move");

    PickList picks;
    presenter.action_picks(play({}), 7, picks);
    CHECK(picks == PickList{ { PickKind::Space, 7, {} } });

    CHECK(presenter.piece_style(TicTacToePresenter::kMark, 0).shape == PieceShape::Cross);
    CHECK(presenter.piece_style(TicTacToePresenter::kMark, 1).glyph == "O");
}

TEST_CASE("TicTacToePresenter reports a win and a draw")
{
    TicTacToePresenter presenter;
    CHECK(describe(presenter, play({ 0, 3, 1, 4, 2 })).status == "X wins");
    CHECK(describe(presenter, play({ 0, 1, 2, 4, 3, 5, 7, 6, 8 })).status == "Draw");
}

TEST_CASE("HexapawnPresenter picks a move as the pawn, then its target")
{
    HexapawnPresenter presenter;
    HexapawnState state;
    BoardView view = describe(presenter, state);
    CHECK(view.pieces.size() == 6);
    CHECK(view.spaces[0].tone != view.spaces[1].tone);
    CHECK(view.status == "White to move");

    PickList picks;
    presenter.action_picks(state, HexapawnState::action_for(6, HexapawnState::Forward), picks);
    CHECK(picks == PickList{ { PickKind::Space, 6, {} }, { PickKind::Space, 3, {} } });
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
    CHECK(plan.mode == LaunchMode::Graphical);
    CHECK(plan.game == "tictactoe");

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
