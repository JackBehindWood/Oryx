#include "doctest.h"

#include "BoardTestSupport.h"

#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

Pick space(uint32_t id)
{
    return { PickKind::Space, id, {} };
}

} // namespace

TEST_CASE("BoardPresentation reports a change only when what is shown or playable changes")
{
    BoardPresentation presentation(create_unique<oasis::TicTacToePresenter>(), "tictactoe", kAllSeats);
    oasis::TicTacToeState state;

    CHECK(presentation.update(state));
    CHECK_FALSE(presentation.update(state));
    CHECK(presentation.changed().empty());
    CHECK(presentation.view().status == "X to move");

    CHECK(presentation.builder().pick(space(4)) == 4);
    state.apply(4);
    CHECK(presentation.update(state));
    CHECK(presentation.changed() == std::vector<SpaceId>{ 4 });
    CHECK(presentation.view().status == "O to move");

    state.undo(4);
    CHECK(presentation.update(state));
    CHECK(presentation.changed() == std::vector<SpaceId>{ 4 });
}

TEST_CASE("BoardPresentation drops a half-built move when the state changes")
{
    BoardPresentation presentation(create_unique<oasis::HexapawnPresenter>(), "hexapawn", kAllSeats);
    oasis::HexapawnState state;
    presentation.update(state);

    CHECK(presentation.builder().pick(space(6)) == PENDING_ACTION);
    CHECK_FALSE(presentation.update(state));
    CHECK(presentation.builder().picked().size() == 1);

    state.apply(oasis::HexapawnState::action_for(7, oasis::HexapawnState::Forward));
    CHECK(presentation.update(state));
    CHECK(presentation.builder().picked().empty());
}

TEST_CASE("BoardPresentation accepts moves only for its seat and shows that seat's view")
{
    DummyGame game(5);
    UniquePtr<IState> state = game.new_initial_state();
    UniquePtr<FakePresenter> owned = create_unique<FakePresenter>();
    FakePresenter& presenter = *owned;
    BoardPresentation presentation(std::move(owned), "dummy", 1);

    presentation.update(*state);
    CHECK(presenter.last_viewer == 1);
    CHECK_FALSE(presentation.accepts_moves());
    CHECK(presentation.builder().empty());

    state->apply(1);
    presentation.update(*state);
    CHECK(presentation.accepts_moves());
    CHECK_FALSE(presentation.builder().empty());

    BoardPresentation hot_seat(create_unique<FakePresenter>(), "dummy", kAllSeats);
    hot_seat.update(*state);
    CHECK(hot_seat.accepts_moves());

    BoardPresentation finished(create_unique<FakePresenter>(), "dummy", kAllSeats);
    finished.update(*finished_dummy_state());
    CHECK(finished.terminal());
    CHECK_FALSE(finished.accepts_moves());
}

TEST_CASE("BoardPresentation rejects an invalid view from its presenter")
{
    class UnlabelledPresenter : public FakePresenter
    {
    public:
        void describe(const IState&, PlayerId, BoardView& out) const override { out.spaces.push_back({}); }
    };

    BoardPresentation presentation(create_unique<UnlabelledPresenter>(), "dummy", kAllSeats);
    CHECK_THROWS_AS(presentation.update(*DummyGame(5).new_initial_state()), Error);
    CHECK_THROWS_AS(BoardPresentation(nullptr, "dummy", kAllSeats), Error);
}

TEST_CASE("build_board_scene resolves styles and highlights for any front end")
{
    BoardPresentation presentation(create_unique<oasis::HexapawnPresenter>(), "hexapawn", kAllSeats);
    oasis::HexapawnState state;
    state.apply(oasis::HexapawnState::action_for(7, oasis::HexapawnState::Forward));
    presentation.update(state);
    presentation.update(state);

    BoardScene scene;
    presentation.build_scene(kNoSpace, scene);
    REQUIRE(scene.spaces.size() == 9);
    CHECK(scene.pieces.size() == 6);
    CHECK(scene.min == Vec2f(-0.5f, -0.5f));
    CHECK(scene.max == Vec2f(2.5f, 2.5f));
    CHECK(scene.status == "Black to move");
    CHECK(scene.spaces[0].highlight == kHighlightNone);

    oasis::HexapawnState fresh;
    BoardPresentation first(create_unique<oasis::HexapawnPresenter>(), "hexapawn", kAllSeats);
    first.update(fresh);
    first.update(state);
    first.build_scene(kNoSpace, scene);
    CHECK(scene.spaces[4].highlight == kHighlightChanged);
    CHECK(scene.spaces[7].highlight == kHighlightChanged);

    first.build_scene(0, scene);
    CHECK((scene.spaces[0].highlight & kHighlightHover) != 0);
    first.build_scene(1, scene);
    CHECK((scene.spaces[1].highlight & kHighlightHover) == 0);

    CHECK(first.builder().pick(space(0)) == PENDING_ACTION);
    first.build_scene(kNoSpace, scene);
    CHECK((scene.spaces[0].highlight & kHighlightPicked) != 0);
    CHECK((scene.spaces[3].highlight & kHighlightTarget) != 0);
    CHECK((scene.spaces[4].highlight & kHighlightTarget) != 0);
    CHECK((scene.spaces[5].highlight & kHighlightTarget) == 0);
    CHECK(scene.options.empty());

    CHECK(scene_columns(scene) == std::vector<float>{ 0.0f, 1.0f, 2.0f });
    CHECK(scene_rows(scene) == std::vector<float>{ 2.0f, 1.0f, 0.0f });
}

TEST_CASE("build_board_scene offers option picks as a menu")
{
    BoardPresentation presentation(create_unique<FakePresenter>(), "dummy", kAllSeats);
    presentation.update(*DummyGame(5).new_initial_state());

    BoardScene scene;
    presentation.build_scene(kNoSpace, scene);
    REQUIRE(scene.options.size() == 3);
    CHECK(scene.options[2].label == "take3");
}
