#include "doctest.h"

#include "../Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

void add_decisions(DashboardFeed& feed, int32_t count)
{
    DummyState state(10);
    for (int32_t i = 0; i < count; ++i)
    {
        Decision decision;
        decision.chosen = 1;
        feed.on_decision(state, decision);
    }
}

} // namespace

TEST_CASE("DashboardModel follows the newest record by default")
{
    FeedOptions options;
    options.capacity = 4;
    DashboardFeed feed(options);
    DashboardModel model;
    CHECK_FALSE(selected_view(model).valid);

    model.feed = &feed;
    CHECK_FALSE(selected_view(model).valid);
    add_decisions(feed, 3);
    CHECK(selected_view(model).record->sequence == 2);
    add_decisions(feed, 1);
    CHECK(selected_view(model).record->sequence == 3);
}

TEST_CASE("DashboardModel keeps an explicit selection until the record leaves the ring")
{
    FeedOptions options;
    options.capacity = 4;
    DashboardFeed feed(options);
    DashboardModel model;
    model.feed = &feed;
    add_decisions(feed, 3);

    select_record(model, 1);
    add_decisions(feed, 2);
    REQUIRE(selected_view(model).valid);
    CHECK(selected_view(model).record->sequence == 1);

    add_decisions(feed, 2);
    CHECK_FALSE(selected_view(model).valid);

    follow_latest_record(model);
    CHECK(selected_view(model).record->sequence == 6);
}

TEST_CASE("DashboardRecord is plain data a GUI Values stride can read")
{
    static_assert(std::is_trivially_copyable_v<DashboardRecord>);
    static_assert(std::is_standard_layout_v<DashboardRecord>);
    CHECK(sizeof(DashboardRecord) % alignof(float) == 0);
}
