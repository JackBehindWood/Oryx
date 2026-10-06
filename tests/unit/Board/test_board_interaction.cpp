#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "Oasis/Game/HexapawnPresenter.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

constexpr SpaceId k_pawn = 6;
constexpr SpaceId k_ahead = 3;

// Moves 1, 2 and 3 of DummyGame as options a, a-b and a-b-c: each is legal and starts the next.
class StopPresenter : public FakePresenter
{
public:
    void action_picks(const IState&, ActionId action, PickList& out) const override
    {
        const char* labels[] = { "a", "b", "c" };
        for (uint32_t step = 0; step < action; ++step)
        {
            out.push_back({ PickKind::Option, step + 1, labels[step] });
        }
    }
};

struct Pawns
{
    Pawns()
        : interaction(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats)
    {
        interaction.update(state);
    }

    HexapawnState state;
    BoardInteraction interaction;
};

} // namespace

TEST_CASE("Pressing a space starts a drag and releasing over a space the move continues to plays it")
{
    Pawns pawns;
    pawns.interaction.set_cursor({ 10.0f, 20.0f });

    pawns.interaction.press(k_pawn);
    CHECK(pawns.interaction.dragging());
    CHECK(pawns.interaction.presentation().builder().picked().size() == 1);
    const BoardScene& scene = pawns.interaction.scene();
    CHECK(scene.drag.active);
    CHECK(scene.drag.space == k_pawn);
    CHECK(scene.drag.cursor == Vec2f(10.0f, 20.0f));
    CHECK(pawns.interaction.poll() == PENDING_ACTION);

    pawns.interaction.release(k_ahead);
    CHECK_FALSE(pawns.interaction.dragging());
    CHECK(pawns.interaction.poll() == HexapawnState::action_for(k_pawn, HexapawnState::Forward));
    CHECK(pawns.interaction.poll() == PENDING_ACTION);
    CHECK_FALSE(pawns.interaction.scene().drag.active);
}

TEST_CASE("Releasing over the pressed space is a click: the selection stays open and the next click completes it")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.release(k_pawn);

    CHECK_FALSE(pawns.interaction.dragging());
    CHECK(pawns.interaction.presentation().builder().picked().size() == 1);
    CHECK(pawns.interaction.poll() == PENDING_ACTION);

    pawns.interaction.press(k_ahead);
    CHECK_FALSE(pawns.interaction.dragging());
    pawns.interaction.release(k_ahead);
    CHECK(pawns.interaction.poll() == HexapawnState::action_for(k_pawn, HexapawnState::Forward));
}

TEST_CASE("Releasing over nothing or over a space the move cannot continue to ends the drag and keeps the selection")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.release(k_no_space);
    CHECK_FALSE(pawns.interaction.dragging());
    CHECK(pawns.interaction.presentation().builder().picked().size() == 1);

    pawns.interaction.press(k_pawn);
    pawns.interaction.release(0);
    CHECK_FALSE(pawns.interaction.dragging());
    CHECK(pawns.interaction.presentation().builder().picked().size() == 1);
    CHECK(pawns.interaction.poll() == PENDING_ACTION);
}

TEST_CASE("A release with no drag does nothing, so a completing press cannot drop into a new move")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.press(k_ahead);
    REQUIRE(pawns.interaction.poll() == HexapawnState::action_for(k_pawn, HexapawnState::Forward));

    pawns.interaction.release(7);
    CHECK(pawns.interaction.presentation().builder().picked().empty());
    CHECK(pawns.interaction.poll() == PENDING_ACTION);
}

TEST_CASE("Back while dragging ends the drag and takes back the pick")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.back();

    CHECK_FALSE(pawns.interaction.dragging());
    CHECK(pawns.interaction.presentation().builder().picked().empty());
    pawns.interaction.release(k_ahead);
    CHECK(pawns.interaction.poll() == PENDING_ACTION);
}

TEST_CASE("Pressing nothing or a space that starts no move drops the picks and starts no drag")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.release(k_pawn);
    pawns.interaction.press(k_no_space);
    CHECK(pawns.interaction.presentation().builder().picked().empty());
    CHECK_FALSE(pawns.interaction.dragging());

    pawns.interaction.press(k_pawn);
    pawns.interaction.release(k_pawn);
    pawns.interaction.press(4);
    CHECK(pawns.interaction.presentation().builder().picked().empty());
    CHECK_FALSE(pawns.interaction.dragging());
}

TEST_CASE("Pressing another piece while one is selected selects it instead")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.release(k_pawn);
    pawns.interaction.press(7);

    CHECK(pawns.interaction.presentation().builder().picked() == PickList{ { PickKind::Space, 7, {} } });
    CHECK(pawns.interaction.dragging());
}

TEST_CASE("Undo drops the picks and any drag, and queues UNDO_ACTION")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.undo();

    CHECK_FALSE(pawns.interaction.dragging());
    CHECK(pawns.interaction.presentation().builder().picked().empty());
    CHECK(pawns.interaction.poll() == UNDO_ACTION);
    CHECK(pawns.interaction.poll() == PENDING_ACTION);
}

TEST_CASE("A state change drops the waiting move and the drag")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.release(k_ahead);

    pawns.state.apply(HexapawnState::action_for(7, HexapawnState::Forward));
    CHECK(pawns.interaction.update(pawns.state));
    CHECK(pawns.interaction.poll() == PENDING_ACTION);

    pawns.interaction.press(0);
    REQUIRE(pawns.interaction.dragging());
    pawns.state.apply(HexapawnState::action_for(0, HexapawnState::Forward));
    CHECK(pawns.interaction.update(pawns.state));
    CHECK_FALSE(pawns.interaction.dragging());
}

TEST_CASE("Intents are ignored on a seat that may not move")
{
    HexapawnState state;
    BoardInteraction black(create_unique<HexapawnPresenter>(), "hexapawn", 1);
    black.update(state);

    black.press(k_pawn);
    black.undo();
    CHECK_FALSE(black.dragging());
    CHECK(black.presentation().builder().picked().empty());
    CHECK(black.poll() == PENDING_ACTION);
}

TEST_CASE("A legal move that could go on is played by confirm, and offered as a Confirm option")
{
    BoardInteraction interaction(create_unique<StopPresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    interaction.update(*state);

    interaction.choose_option(0);
    CHECK(interaction.presentation().builder().ready());
    CHECK(interaction.poll() == PENDING_ACTION);
    const BoardScene& scene = interaction.scene();
    REQUIRE(scene.options.size() == 2);
    CHECK(scene.options[0].label == "b");
    CHECK(scene.options[1].kind == PickKind::Confirm);

    interaction.confirm();
    CHECK(interaction.poll() == 1);
    CHECK(interaction.presentation().builder().picked().empty());
}

TEST_CASE("Choosing the Confirm option plays the move, and continuing plays the longer one")
{
    BoardInteraction interaction(create_unique<StopPresenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    interaction.update(*state);

    interaction.choose_option(0);
    interaction.choose_option(0);
    REQUIRE(interaction.scene().options.size() == 2);
    interaction.choose_option(1);
    CHECK(interaction.poll() == 2);

    interaction.choose_option(0);
    interaction.choose_option(0);
    interaction.choose_option(0);
    CHECK(interaction.poll() == 3);
}

TEST_CASE("Confirm does nothing while the move is not yet legal")
{
    Pawns pawns;
    pawns.interaction.press(k_pawn);
    pawns.interaction.confirm();
    CHECK(pawns.interaction.presentation().builder().picked().size() == 1);
    CHECK(pawns.interaction.poll() == PENDING_ACTION);
}
