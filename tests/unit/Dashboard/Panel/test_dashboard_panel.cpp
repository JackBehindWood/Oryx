#include "doctest.h"

#include "Oryx.h"
#include "unit/Game/DummyGame.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct PanelFixture : GuiFixture
{
    DashboardFeed feed;
    DashboardModel model;
    DashboardPanelState state;

    explicit PanelFixture(const std::vector<std::string>& ids = { "probabilities", "values" })
        : state(make_panel_state(ids))
    {
        driver.input().surface_size = { 300.0f, 500.0f };
        model.feed = &feed;
    }

    void frames(uint32_t count = 3)
    {
        driver.run_frames(count, column_of([this] { draw_dashboard(state, model); }));
    }

    void add_decision()
    {
        UniquePtr<IGame> game = GameRegistry::create("tictactoe");
        REQUIRE(game != nullptr);
        UniquePtr<IState> position = game->new_initial_state();
        ActionList actions = position->legal_actions();
        Decision decision;
        decision.player = position->current_player();
        decision.chosen = actions[0];
        for (ActionId action : actions)
        {
            set_probability(decision, action, 1.0 / static_cast<double>(actions.size()));
            set_value(decision, action, action == decision.chosen ? 0.5 : -0.25);
        }
        feed.on_decision(*position, decision);
    }
};

} // namespace

TEST_CASE("dashboard panel: views are created in the order of the ids and an unknown id is skipped")
{
    const DashboardPanelState state = make_panel_state({ "values", "no/such/view", "probabilities" });
    REQUIRE(state.views.size() == 2);
    UniquePtr<IDashboardView> values = DashboardViewRegistry::create("values");
    UniquePtr<IDashboardView> probabilities = DashboardViewRegistry::create("probabilities");
    CHECK(state.views[0]->title() == values->title());
    CHECK(state.views[1]->title() == probabilities->title());
}

TEST_CASE("dashboard panel: empty states")
{
    PanelFixture none({});
    none.frames();
    CHECK(none.find_text("No dashboard views") != nullptr);

    PanelFixture detached;
    detached.model.feed = nullptr;
    detached.frames();
    CHECK(detached.find_text("No active match") != nullptr);
    CHECK(detached.find_text(detached.state.views[0]->title()) == nullptr);

    PanelFixture waiting;
    waiting.frames();
    CHECK(waiting.find_text("No decision to show") != nullptr);
}

TEST_CASE("dashboard panel: every view sits under its own header in id order")
{
    PanelFixture f;
    f.add_decision();
    f.frames();
    const LayoutNode* first = f.find_text(f.state.views[0]->title());
    const LayoutNode* second = f.find_text(f.state.views[1]->title());
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK(first->rect.min[1] < second->rect.min[1]);
}

TEST_CASE("dashboard panel: collapsing a header hides its view")
{
    PanelFixture f;
    f.add_decision();
    f.frames();
    CHECK(f.find_text("No decision to show") == nullptr);
    const LayoutNode* header = f.find_text(f.state.views[0]->title());
    REQUIRE(header != nullptr);
    const std::string before = dump_layout(f.context);
    f.driver.click(rect_centre(header->rect), f.column_of([&] { draw_dashboard(f.state, f.model); }));
    f.frames();
    CHECK(dump_layout(f.context) != before);
}

TEST_CASE("dashboard panel: the header counts matches and decisions")
{
    PanelFixture f;
    f.frames();
    CHECK(f.find_text("Waiting for the first decision") != nullptr);
    f.add_decision();
    f.frames();
    CHECK(f.find_text("Waiting for the first decision") == nullptr);
    CHECK(f.find_text("Decisions") != nullptr);
}

TEST_CASE("dashboard panel: layout golden for a populated feed")
{
    PanelFixture f;
    f.add_decision();
    f.driver.settle(f.column_of([&] { draw_dashboard(f.state, f.model); }));
    const std::string first = dump_layout(f.context);
    f.frames();
    CHECK(dump_layout(f.context) == first);
    CHECK_FALSE(first.empty());
}

TEST_CASE("dashboard panel: a warm frame with a populated feed allocates nothing")
{
    PanelFixture f;
    f.add_decision();
    f.add_decision();
    f.frames(6);
    const MemoryStats before = test::all_allocations();
    f.frames(3);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}
