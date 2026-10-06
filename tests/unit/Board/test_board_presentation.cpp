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
    BoardPresentation presentation(create_unique<oasis::TicTacToePresenter>(), "tictactoe", k_all_seats);
    oasis::TicTacToeState state;

    CHECK(presentation.update(state));
    CHECK_FALSE(presentation.update(state));
    CHECK(presentation.changed().empty());
    CHECK(presentation.view().status == "X to move");

    CHECK(presentation.builder().pick(space(4)).action == 4);
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
    BoardPresentation presentation(create_unique<oasis::HexapawnPresenter>(), "hexapawn", k_all_seats);
    oasis::HexapawnState state;
    presentation.update(state);

    CHECK(presentation.builder().pick(space(6)).status == PickStatus::Pending);
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

    BoardPresentation hot_seat(create_unique<FakePresenter>(), "dummy", k_all_seats);
    hot_seat.update(*state);
    CHECK(hot_seat.accepts_moves());

    BoardPresentation finished(create_unique<FakePresenter>(), "dummy", k_all_seats);
    finished.update(*finished_dummy_state());
    CHECK(finished.terminal());
    CHECK_FALSE(finished.accepts_moves());
}

TEST_CASE("BoardPresentation rejects an invalid view from its presenter")
{
    class StrayPiecePresenter : public FakePresenter
    {
    public:
        void describe_pieces(const IState&, PlayerId, BoardContent& out) const override { out.pieces.push_back({ 0, 0, 7 }); }
    };

    BoardPresentation presentation(create_unique<StrayPiecePresenter>(), "dummy", k_all_seats);
    CHECK_THROWS_AS(presentation.update(*DummyGame(5).new_initial_state()), Error);
    CHECK_THROWS_AS(BoardPresentation(nullptr, "dummy", k_all_seats), Error);
}

TEST_CASE("build_board_scene resolves styles and highlights for any front end")
{
    BoardPresentation presentation(create_unique<oasis::HexapawnPresenter>(), "hexapawn", k_all_seats);
    oasis::HexapawnState state;
    state.apply(oasis::HexapawnState::action_for(7, oasis::HexapawnState::Forward));
    presentation.update(state);
    presentation.update(state);

    BoardScene scene;
    presentation.build_scene(k_no_space, scene);
    REQUIRE(scene.highlights.size() == 9);
    CHECK(scene.pieces.size() == 6);
    CHECK(scene.layout->min() == Vec2f(-0.5f, -0.5f));
    CHECK(scene.layout->max() == Vec2f(2.5f, 2.5f));
    CHECK(scene.status == "Black to move");
    CHECK(scene.highlights[0] == SpaceHighlight::None);

    oasis::HexapawnState fresh;
    BoardPresentation first(create_unique<oasis::HexapawnPresenter>(), "hexapawn", k_all_seats);
    first.update(fresh);
    first.update(state);
    first.build_scene(k_no_space, scene);
    CHECK(scene.highlights[4] == SpaceHighlight::Changed);
    CHECK(scene.highlights[7] == SpaceHighlight::Changed);

    first.build_scene(0, scene);
    CHECK(has_highlight(scene.highlights[0], SpaceHighlight::Hover));
    first.build_scene(1, scene);
    CHECK(!has_highlight(scene.highlights[1], SpaceHighlight::Hover));

    CHECK(first.builder().pick(space(0)).status == PickStatus::Pending);
    first.build_scene(k_no_space, scene);
    CHECK(has_highlight(scene.highlights[0], SpaceHighlight::Picked));
    CHECK(has_highlight(scene.highlights[3], SpaceHighlight::Target));
    CHECK(has_highlight(scene.highlights[4], SpaceHighlight::Target));
    CHECK(!has_highlight(scene.highlights[5], SpaceHighlight::Target));
    CHECK(scene.options.empty());

    CHECK(scene.layout->columns() == std::vector<float>{ 0.0f, 1.0f, 2.0f });
    CHECK(scene.layout->rows() == std::vector<float>{ 2.0f, 1.0f, 0.0f });
}

TEST_CASE("build_board_scene offers option picks as a menu")
{
    BoardPresentation presentation(create_unique<FakePresenter>(), "dummy", k_all_seats);
    presentation.update(*DummyGame(5).new_initial_state());

    BoardScene scene;
    presentation.build_scene(k_no_space, scene);
    REQUIRE(scene.options.size() == 3);
    CHECK(scene.options[2].label == "take3");
}

namespace
{

// Shows a piece that only seat 0 may see, and records every viewer it was asked to describe for.
class SecretPresenter : public FakePresenter
{
public:
    void describe_pieces(const IState& state, PlayerId viewer, BoardContent& out) const override
    {
        FakePresenter::describe_pieces(state, viewer, out);
        viewers.push_back(viewer);
        if (viewer == 0)
        {
            out.pieces.push_back({ 0, 0, 0 });
        }
    }

    mutable std::vector<PlayerId> viewers;
};

} // namespace

TEST_CASE("BoardPresentation describes the table for its seat, or for whoever is to move in hot-seat, and never shows a seat what it cannot see")
{
    UniquePtr<IState> state = DummyGame(5).new_initial_state();

    auto shown = [&](PlayerId seat, const IState& at)
    {
        UniquePtr<SecretPresenter> presenter = create_unique<SecretPresenter>();
        SecretPresenter* raw = presenter.get();
        BoardPresentation presentation(std::move(presenter), "dummy", seat);
        presentation.update(at);
        CHECK(raw->viewers == std::vector<PlayerId>{ seat == k_all_seats ? at.current_player() : seat });
        return presentation.view().pieces.size();
    };

    CHECK(shown(0, *state) == 1);
    CHECK(shown(1, *state) == 0);
    CHECK(shown(k_all_seats, *state) == 1);

    state->apply(1);
    CHECK(shown(0, *state) == 1);
    CHECK(shown(1, *state) == 0);
    CHECK(shown(k_all_seats, *state) == 0);
}
