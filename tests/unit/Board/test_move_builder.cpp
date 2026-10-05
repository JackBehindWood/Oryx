#include "doctest.h"

#include "Oryx.h"

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

} // namespace

TEST_CASE("MoveBuilder completes a one-pick move immediately")
{
    MoveBuilder builder;
    builder.reset({ { 7, { space(7) } }, { 8, { space(8) } } });

    CHECK(builder.pick(space(8)) == 8);
    CHECK(builder.picked().empty());
    CHECK(builder.pick(space(3)) == INVALID_ACTION);
}

TEST_CASE("MoveBuilder narrows multi-pick moves and offers the next picks")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK(builder.next_picks() == PickList{ space(0), space(3), space(5) });
    CHECK(builder.pick(space(0)) == PENDING_ACTION);
    CHECK(builder.picked() == PickList{ space(0) });
    CHECK(builder.next_picks() == PickList{ space(1), space(2) });
    CHECK(builder.can_pick(space(2)));
    CHECK_FALSE(builder.can_pick(space(4)));
    CHECK(builder.pick(space(2)) == 11);
    CHECK(builder.picked().empty());
}

TEST_CASE("MoveBuilder offers options as the last step and keeps their labels")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK(builder.pick(space(3)) == PENDING_ACTION);
    CHECK(builder.pick(space(4)) == PENDING_ACTION);
    PickList next = builder.next_picks();
    REQUIRE(next.size() == 2);
    CHECK(next[0].kind == PickKind::Option);
    CHECK(next[1].label == "knight");
    CHECK(builder.pick(option(1, "")) == 13);
}

TEST_CASE("MoveBuilder starts again from a pick that begins another move, and keeps its picks on a pick that fits nothing")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK(builder.pick(space(0)) == PENDING_ACTION);
    CHECK(builder.pick(space(5)) == PENDING_ACTION);
    CHECK(builder.picked() == PickList{ space(5) });

    CHECK(builder.pick(space(9)) == INVALID_ACTION);
    CHECK(builder.picked() == PickList{ space(5) });
    CHECK(builder.pick(space(1)) == 14);
}

TEST_CASE("MoveBuilder takes back picks one at a time")
{
    MoveBuilder builder;
    builder.reset(chess_like());

    CHECK_FALSE(builder.back());
    CHECK(builder.pick(space(3)) == PENDING_ACTION);
    CHECK(builder.pick(space(4)) == PENDING_ACTION);
    CHECK(builder.back());
    CHECK(builder.picked() == PickList{ space(3) });
    builder.clear();
    CHECK(builder.picked().empty());
}

TEST_CASE("MoveBuilder rejects candidates a person could not tell apart")
{
    MoveBuilder builder;
    CHECK_THROWS_AS(builder.reset({ { 1, { space(0) } }, { 2, { space(0), space(1) } } }), Error);
    CHECK_THROWS_AS(builder.reset({ { 1, { space(0) } }, { 2, { space(0) } } }), Error);
    CHECK_THROWS_AS(builder.reset({ { 1, {} } }), Error);

    builder.reset({});
    CHECK(builder.empty());
    CHECK(builder.next_picks().empty());
    CHECK(builder.pick(space(0)) == INVALID_ACTION);
}
