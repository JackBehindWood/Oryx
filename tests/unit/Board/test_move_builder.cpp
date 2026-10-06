#include "doctest.h"

#include "Oryx.h"
#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;

namespace
{

Pick space(uint32_t id)
{
    return { PickKind::Space, id, {} };
}

Pick option(uint32_t id, std::string label)
{
    return { PickKind::Option, id, std::move(label) };
}

// From-to moves with a promotion menu on one of them, as a chess-like game would present them.
std::vector<MoveCandidate> chess_like()
{
    return {
        { 10, { space(0), space(1) } },
        { 11, { space(0), space(2) } },
        { 12, { space(3), space(4), option(0, "queen") } },
        { 13, { space(3), space(4), option(1, "knight") } },
        { 14, { space(5), space(1) } },
    };
}

// A jump that may stop after any hop: 2 is a legal move and also the start of 3.
std::vector<MoveCandidate> hop_like()
{
    return {
        { 1, { space(0), space(1) } },
        { 2, { space(0), space(2) } },
        { 3, { space(0), space(2), space(3) } },
    };
}

PickStatus status_of(MoveBuilder& builder, const Pick& pick)
{
    return builder.pick(pick).status;
}

} // namespace

TEST_CASE("MoveBuilder completes a one-pick move immediately")
{
    MoveBuilder builder;
    builder.reset({ { 7, { space(7) } }, { 8, { space(8) } } });

    PickResult done = builder.pick(space(8));
    CHECK(done.status == PickStatus::Complete);
    CHECK(done.action == 8);
    CHECK(builder.picked().empty());
    PickResult none = builder.pick(space(3));
    CHECK(none.status == PickStatus::Rejected);
    CHECK(none.action == INVALID_ACTION);
}

TEST_CASE("MoveBuilder narrows multi-pick moves and offers the next picks")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK(builder.next_picks() == PickList{ space(0), space(3), space(5) });
    CHECK(status_of(builder, space(0)) == PickStatus::Pending);
    CHECK(builder.picked() == PickList{ space(0) });
    CHECK(builder.next_picks() == PickList{ space(1), space(2) });
    CHECK(builder.can_pick(space(2)));
    CHECK_FALSE(builder.can_pick(space(4)));
    CHECK(builder.pick(space(2)).action == 11);
    CHECK(builder.picked().empty());
}

TEST_CASE("MoveBuilder offers options as the last step and keeps their labels")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK(status_of(builder, space(3)) == PickStatus::Pending);
    CHECK(status_of(builder, space(4)) == PickStatus::Pending);
    PickList next = builder.next_picks();
    REQUIRE(next.size() == 2);
    CHECK(next[0].kind == PickKind::Option);
    CHECK(next[1].label == "knight");
    CHECK(builder.pick(option(1, "")).action == 13);
}

TEST_CASE("MoveBuilder starts again from a pick that begins another move, and keeps its picks on a pick that fits nothing")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK(status_of(builder, space(0)) == PickStatus::Pending);
    CHECK(status_of(builder, space(5)) == PickStatus::Pending);
    CHECK(builder.picked() == PickList{ space(5) });

    CHECK(status_of(builder, space(9)) == PickStatus::Rejected);
    CHECK(builder.picked() == PickList{ space(5) });
    CHECK(builder.pick(space(1)).action == 14);
}

TEST_CASE("MoveBuilder takes back picks one at a time")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK_FALSE(builder.back());
    CHECK(status_of(builder, space(3)) == PickStatus::Pending);
    CHECK(status_of(builder, space(4)) == PickStatus::Pending);
    CHECK(builder.back());
    CHECK(builder.picked() == PickList{ space(3) });
    builder.clear();
    CHECK(builder.picked().empty());
}

TEST_CASE("MoveBuilder rejects candidates with equal picks, none, or a Confirm pick, but lets one start another")
{
    MoveBuilder builder;
    CHECK_THROWS_AS(builder.reset({ { 1, { space(0) } }, { 2, { space(0) } } }), Error);
    CHECK_THROWS_AS(builder.reset({ { 1, {} } }), Error);
    CHECK_THROWS_AS(builder.reset({ { 1, { space(0), { PickKind::Confirm, 0, {} } } } }), Error);
    CHECK(builder.empty());
    CHECK_NOTHROW(builder.reset({ { 1, { space(0) } }, { 2, { space(0), space(1) } } }));

    builder.reset({});
    CHECK(builder.empty());
    CHECK(builder.next_picks().empty());
    CHECK(status_of(builder, space(0)) == PickStatus::Rejected);
}

TEST_CASE("MoveBuilder lists children in the order their picks first appear")
{
    MoveBuilder builder;
    builder.reset({ { 1, { space(5), space(9) } }, { 2, { space(2) } }, { 3, { space(5), space(1) } }, { 4, { space(7) } } });

    CHECK(builder.next_picks() == PickList{ space(5), space(2), space(7) });
    CHECK(status_of(builder, space(5)) == PickStatus::Pending);
    CHECK(builder.next_picks() == PickList{ space(9), space(1) });
}

TEST_CASE("MoveBuilder treats a move that others extend as Ready, then either confirms or continues")
{
    MoveBuilder builder;
    builder.reset(hop_like());
    REQUIRE(status_of(builder, space(0)) == PickStatus::Pending);
    CHECK_FALSE(builder.ready());

    PickResult stop = builder.pick(space(2));
    CHECK(stop.status == PickStatus::Ready);
    CHECK(stop.action == 2);
    CHECK(builder.ready());
    CHECK(builder.picked() == PickList{ space(0), space(2) });
    PickList next = builder.next_picks();
    REQUIRE(next.size() == 2);
    CHECK(next[0] == space(3));
    CHECK(next[1].kind == PickKind::Confirm);

    PickResult confirmed = builder.confirm();
    CHECK(confirmed.status == PickStatus::Complete);
    CHECK(confirmed.action == 2);
    CHECK(builder.picked().empty());

    builder.pick(space(0));
    builder.pick(space(2));
    PickResult go_on = builder.pick(space(3));
    CHECK(go_on.status == PickStatus::Complete);
    CHECK(go_on.action == 3);
    CHECK(builder.picked().empty());
}

TEST_CASE("MoveBuilder takes a Confirm pick as confirm() and rejects it when no move is ready")
{
    MoveBuilder builder;
    builder.reset(hop_like());
    builder.pick(space(0));

    CHECK(status_of(builder, { PickKind::Confirm, 0, {} }) == PickStatus::Rejected);
    CHECK_FALSE(builder.can_pick({ PickKind::Confirm, 0, {} }));
    CHECK(builder.picked() == PickList{ space(0) });
    CHECK(builder.confirm().status == PickStatus::Rejected);

    builder.pick(space(2));
    CHECK(builder.can_pick({ PickKind::Confirm, 0, {} }));
    PickResult result = builder.pick({ PickKind::Confirm, 0, {} });
    CHECK(result.status == PickStatus::Complete);
    CHECK(result.action == 2);
}

TEST_CASE("MoveBuilder can go back from a Ready move to the pick before it")
{
    MoveBuilder builder;
    builder.reset(hop_like());
    builder.pick(space(0));
    builder.pick(space(2));

    CHECK(builder.back());
    CHECK_FALSE(builder.ready());
    CHECK(builder.next_picks() == PickList{ space(1), space(2) });
}

TEST_CASE("MoveBuilder::preview lists what would follow a pick and changes nothing")
{
    MoveBuilder builder;
    builder.reset(hop_like());

    CHECK(builder.preview(space(0)) == PickList{ space(1), space(2) });
    CHECK(builder.picked().empty());
    CHECK(builder.preview(space(1)).empty());
    CHECK(builder.preview(space(9)).empty());

    builder.pick(space(0));
    PickList after_stop = builder.preview(space(2));
    REQUIRE(after_stop.size() == 2);
    CHECK(after_stop[0] == space(3));
    CHECK(after_stop[1].kind == PickKind::Confirm);
    CHECK(builder.picked() == PickList{ space(0) });
    CHECK(builder.next_picks() == PickList{ space(1), space(2) });
}

TEST_CASE("MoveBuilder::preview and can_pick follow the re-select rule")
{
    MoveBuilder builder;
    builder.reset(chess_like());
    builder.pick(space(0));

    CHECK(builder.can_pick(space(5)));
    CHECK(builder.preview(space(5)) == PickList{ space(1) });
    CHECK_FALSE(builder.can_pick(space(9)));
    CHECK(builder.picked() == PickList{ space(0) });
}

TEST_CASE("MoveBuilder plays every legal action of real games from its own picks")
{
    uint32_t seed = 12345;
    auto roll = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed >> 8; };

    auto replay_all = [](const IBoardPresenter& presenter, const IState& state)
    {
        std::vector<MoveCandidate> candidates = collect_candidates(presenter, state);
        MoveBuilder builder;
        REQUIRE_NOTHROW(builder.reset(candidates));
        for (const MoveCandidate& candidate : candidates)
        {
            PickResult result;
            for (const Pick& pick : candidate.picks)
            {
                result = builder.pick(pick);
                CHECK(result.status != PickStatus::Rejected);
            }
            CHECK(result.status == PickStatus::Complete);
            CHECK(result.action == candidate.action);
            CHECK(builder.picked().empty());
        }
    };

    for (uint32_t game = 0; game < 25; ++game)
    {
        oasis::TicTacToeState tic;
        oasis::TicTacToePresenter tic_presenter;
        oasis::HexapawnState pawns;
        oasis::HexapawnPresenter pawn_presenter;
        while (!tic.is_terminal())
        {
            replay_all(tic_presenter, tic);
            ActionList legal = tic.legal_actions();
            tic.apply(legal[roll() % legal.size()]);
        }
        while (!pawns.is_terminal())
        {
            replay_all(pawn_presenter, pawns);
            ActionList legal = pawns.legal_actions();
            pawns.apply(legal[roll() % legal.size()]);
        }
    }
}

TEST_CASE("MoveBuilder steps, previews and lists picks without allocating once warm")
{
    MoveBuilder builder;
    builder.reset(chess_like());
    PickList next;
    PickList preview;
    builder.next_picks(next);
    builder.preview(space(3), preview);

    MemoryStats before = test::all_allocations();
    for (uint32_t round = 0; round < 100; ++round)
    {
        builder.pick(space(3));
        builder.pick(space(4));
        builder.next_picks(next);
        builder.preview(space(5), preview);
        builder.can_pick(option(1, ""));
        builder.back();
        builder.back();
        builder.pick(space(0));
        builder.pick(space(2));
        builder.clear();
    }
    MemoryStats delta = memory_delta(before, test::all_allocations());

    CHECK(delta.allocation_count == 0);
}
