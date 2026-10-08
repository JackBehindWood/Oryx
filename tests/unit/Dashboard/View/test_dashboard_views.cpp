#include "doctest.h"

#include "NullRHI.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/Game/DummyGame.h"
#include "unit/Interface/support/GuiFixture.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct ViewFixture : GuiFixture
{
    DashboardFeed feed;
    DashboardModel model;
    UniquePtr<IDashboardView> view;

    explicit ViewFixture(std::string_view id, const FeedOptions& options = FeedOptions())
        : feed(options)
    {
        driver.input().surface_size = { 300.0f, 400.0f };
        model.feed = &feed;
        view = DashboardViewRegistry::create(std::string(id));
        REQUIRE(view != nullptr);
    }

    void frames(uint32_t count = 3)
    {
        driver.run_frames(count, column_of([this] { view->draw(model); }));
    }

    void settle()
    {
        driver.settle(column_of([this] { view->draw(model); }));
    }

    const LayoutNode* find_named(std::string_view name) const
    {
        for (uint32_t index = 0; index < context.layout().node_count(); ++index)
        {
            if (context.layout().node(index).name == name)
            {
                return &context.layout().node(index);
            }
        }
        return nullptr;
    }
};

void feed_uniform(DashboardFeed& feed, const std::string& game_id, int32_t chosen_index)
{
    UniquePtr<IGame> game = GameRegistry::create(game_id);
    REQUIRE(game != nullptr);
    UniquePtr<IState> state = game->new_initial_state();
    ActionList actions = state->legal_actions();
    Decision decision;
    decision.player = state->current_player();
    decision.chosen = actions[static_cast<size_t>(chosen_index)];
    for (ActionId action : actions)
    {
        set_probability(decision, action, 1.0 / static_cast<double>(actions.size()));
        set_value(decision, action, action == decision.chosen ? 0.5 : -0.25);
    }
    feed.on_decision(*state, decision);
}

std::string first_label(const DashboardFeed& feed)
{
    RecordView view = feed.view(feed.size() - 1);
    return std::string(label_view(view.scores[0].label));
}

} // namespace

TEST_CASE("dashboard view: the built-in views register by id and an unknown id is null")
{
    for (const char* id : { "probabilities", "values" })
    {
        UniquePtr<IDashboardView> view = DashboardViewRegistry::create(id);
        REQUIRE(view != nullptr);
        CHECK_FALSE(view->title().empty());
    }
    CHECK(DashboardViewRegistry::create("no/such/view") == nullptr);
}

TEST_CASE("dashboard view: a view written outside the library registers and draws")
{
    class CountView final : public IDashboardView
    {
    public:
        [[nodiscard]] std::string_view title() const override { return "Count"; }
        void draw(DashboardModel& model) override { gui::label(gui::context().arena().format("records %u", static_cast<uint32_t>(model.feed->size()))); }
    };
    DashboardViewRegistry::register_factory("test/count", [](const Params&) -> UniquePtr<IDashboardView> { return create_unique<CountView>(); });
    ViewFixture f("test/count");
    feed_uniform(f.feed, "tictactoe", 0);
    f.frames();
    CHECK(f.find_text("records 1") != nullptr);
    CHECK(DashboardViewRegistry::unregister_factory("test/count"));
}

TEST_CASE("dashboard view: empty states say why nothing is drawn")
{
    for (const char* id : { "probabilities", "values" })
    {
        ViewFixture f(id);
        f.frames();
        CHECK(f.find_text("No decision to show") != nullptr);
        f.model.feed = nullptr;
        f.frames();
        CHECK(f.find_text("No decision to show") != nullptr);
    }

    ViewFixture probabilities("probabilities");
    DummyState state(10);
    Decision values_only;
    values_only.chosen = 1;
    set_value(values_only, 1, 0.5);
    probabilities.feed.on_decision(state, values_only);
    probabilities.frames();
    CHECK(probabilities.find_text("Strategy reports no probabilities") != nullptr);

    ViewFixture values("values");
    Decision probabilities_only;
    probabilities_only.chosen = 1;
    set_probability(probabilities_only, 1, 1.0);
    values.feed.on_decision(state, probabilities_only);
    values.frames();
    CHECK(values.find_text("Strategy reports no values") != nullptr);
}

TEST_CASE("dashboard view: probabilities show every game's own action labels")
{
    for (const char* game_id : { "tictactoe", "hexapawn" })
    {
        INFO(game_id);
        ViewFixture f("probabilities");
        feed_uniform(f.feed, game_id, 1);
        f.settle();
        f.frames();
        RecordView record = f.feed.view(0);
        REQUIRE(record.valid);
        CHECK(record.record->score_count > 1);
        CHECK(f.find_text(label_view(record.scores[0].label)) != nullptr);
        CHECK(f.find_text(label_view(record.scores[1].label)) != nullptr);
        CHECK(f.find_text("Decision 0, player 0, chose " + std::string(label_view(record.record->chosen_label))) != nullptr);
    }
}

TEST_CASE("dashboard view: a chess-sized action space is drawn from the same code")
{
    ViewFixture f("probabilities");
    DummyState state(10);
    Decision decision;
    decision.chosen = 7;
    for (ActionId action = 0; action < 64; ++action)
    {
        set_probability(decision, action, 1.0 / 64.0);
    }
    f.feed.on_decision(state, decision);
    CHECK(f.feed.drops().scores == 0);
    f.settle();
    f.frames();
    CHECK(f.find_text("take 0") != nullptr);
    CHECK(f.find_text("1.6%") != nullptr);
}

TEST_CASE("dashboard view: values print the raw number whatever the strategy's scale")
{
    DummyState state(10);
    for (double scale : { 1.0, 300.0 })
    {
        ViewFixture f("values");
        Decision decision;
        decision.chosen = 1;
        set_value(decision, 1, 0.5 * scale);
        set_value(decision, 2, -0.25 * scale);
        f.feed.on_decision(state, decision);
        f.settle();
        f.frames();
        CHECK(f.find_text(f.context.arena().format("%.2f", 0.5 * scale)) != nullptr);
        CHECK(f.find_text(f.context.arena().format("%.2f", -0.25 * scale)) != nullptr);
    }
}

TEST_CASE("dashboard view: the value history keeps order across a ring wrap and a missing value is a gap")
{
    FeedOptions options;
    options.capacity = 4;
    ViewFixture f("values", options);
    DummyState state(10);
    for (int32_t i = 0; i < 6; ++i)
    {
        Decision decision;
        decision.chosen = 1;
        if (i != 3)
        {
            set_value(decision, 1, static_cast<double>(i));
        }
        f.feed.on_decision(state, decision);
    }
    CHECK(f.feed.size() == 4);
    f.settle();
    f.frames();
    CHECK(f.find_named("chosen value") != nullptr);
    CHECK(std::isnan(f.feed.view(1).record->chosen_value));
}

TEST_CASE("dashboard view: clicking the value history selects that record")
{
    ViewFixture f("values");
    DummyState state(10);
    for (int32_t i = 0; i < 8; ++i)
    {
        Decision decision;
        decision.chosen = 1;
        set_value(decision, 1, static_cast<double>(i));
        f.feed.on_decision(state, decision);
    }
    f.settle();
    f.frames();
    const LayoutNode* plot = f.find_named("chosen value");
    REQUIRE(plot != nullptr);
    const Vec2f left = { plot->rect.min[0] + 2.0f, plot->rect.min[1] + plot->rect.size[1] * 0.5f };
    f.driver.click(left, f.column_of([&f] { f.view->draw(f.model); }));
    CHECK_FALSE(f.model.follow_latest);
    CHECK(f.model.selected_sequence < 4);
}

TEST_CASE("dashboard view: both views allocate nothing once warm")
{
    for (const char* id : { "probabilities", "values" })
    {
        INFO(id);
        ViewFixture f(id);
        for (int32_t i = 0; i < 5; ++i)
        {
            feed_uniform(f.feed, "tictactoe", i);
        }
        f.settle();
        f.frames(4);
        const MemoryStats before = test::all_allocations();
        f.frames(3);
        CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
    }
}

TEST_CASE("dashboard view: a view's frame replays through the null renderer")
{
    for (const char* id : { "probabilities", "values" })
    {
        INFO(id);
        UniquePtr<RendererContext> renderer = create_renderer_context({ RHIBackend::Null });
        ViewFixture f(id);
        feed_uniform(f.feed, "hexapawn", 0);
        f.settle();
        f.frames();

        std::vector<DrawItem> sink;
        BatchRenderer2D batcher{ batch_renderer_desc(*renderer, sink) };
        Camera2D camera = Camera2D::screen_space(300.0f, 400.0f);
        batcher.begin(camera, { sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { 300.0f, 400.0f } });
        replay(f.context.draw_list(), batcher, f.font, ReplayTarget{ { 0.0f, 0.0f }, { 300.0f, 400.0f } });
        batcher.end();
        CHECK_FALSE(sink.empty());
        renderer->rhi->wait_idle();
    }
}
