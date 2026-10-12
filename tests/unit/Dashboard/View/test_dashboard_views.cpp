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

    bool painted(std::string_view text) const
    {
        const DrawList& list = context.draw_list();
        for (uint32_t channel = 0; channel < list.channel_count(); ++channel)
        {
            for (const TextCmd& command : list.channel(channel).texts)
            {
                if (list.text(command) == text)
                {
                    return true;
                }
            }
        }
        return false;
    }

    std::vector<std::string> painted_texts() const
    {
        std::vector<std::string> texts;
        const DrawList& list = context.draw_list();
        for (uint32_t channel = 0; channel < list.channel_count(); ++channel)
        {
            for (const TextCmd& command : list.channel(channel).texts)
            {
                texts.emplace_back(list.text(command));
            }
        }
        return texts;
    }

    std::vector<RectCmd> painted_rects(const Colour& rgb) const
    {
        std::vector<RectCmd> rects;
        const DrawList& list = context.draw_list();
        for (uint32_t channel = 0; channel < list.channel_count(); ++channel)
        {
            for (const RectCmd& command : list.channel(channel).rects)
            {
                if (command.colour.r == rgb.r && command.colour.g == rgb.g && command.colour.b == rgb.b)
                {
                    rects.push_back(command);
                }
            }
        }
        return rects;
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
    add_metric(decision.extra, "search/nodes", 12.0);
    max_metric(decision.extra, "search/depth_max", 3.0);
    add_metric(decision.extra, "plain", 1.0);
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
    for (const char* id : { "probabilities", "values", "decision", "trace", "stats" })
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
    for (const char* id : { "probabilities", "values", "decision" })
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
        CHECK(f.painted(label_view(record.scores[0].label)));
        CHECK(f.painted(label_view(record.scores[1].label)));
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
    CHECK(f.painted("take 0"));
    CHECK(f.painted("1.6%"));
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
        CHECK(f.painted(scale == 1.0 ? "+0.50" : "+150"));
        CHECK(f.painted(scale == 1.0 ? "-0.25" : "-75.00"));
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

TEST_CASE("dashboard view: every view allocates nothing once warm")
{
    for (const char* id : { "probabilities", "values", "decision", "trace", "stats" })
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
    for (const char* id : { "probabilities", "values", "decision", "trace", "stats" })
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

namespace
{

void feed_numbered(DashboardFeed& feed, int32_t count, int32_t first = 0)
{
    DummyState state(10);
    for (int32_t i = first; i < first + count; ++i)
    {
        Decision decision;
        decision.chosen = static_cast<ActionId>(i);
        set_value(decision, decision.chosen, static_cast<double>(i));
        feed.on_decision(state, decision);
    }
}

uint32_t count_text_prefix(const GuiContext& context, std::string_view prefix)
{
    uint32_t count = 0;
    for (uint32_t index = 0; index < context.layout().node_count(); ++index)
    {
        count += context.layout().node(index).paint.text.starts_with(prefix) ? 1u : 0u;
    }
    return count;
}

} // namespace

TEST_CASE("dashboard trace: only the visible rows are submitted whatever the history size")
{
    ViewFixture f("trace");
    feed_numbered(f.feed, 1000);
    f.settle();
    f.frames();
    const uint32_t rows = count_text_prefix(f.context, "take ");
    CHECK(rows > 0);
    CHECK(rows < 40);
}

TEST_CASE("dashboard trace: following keeps the newest row in view as decisions arrive")
{
    ViewFixture f("trace");
    feed_numbered(f.feed, 400);
    f.settle();
    f.frames();
    CHECK(f.find_text("take 399") != nullptr);
    for (int32_t i = 400; i < 405; ++i)
    {
        feed_numbered(f.feed, 1, i);
        f.frames(3);
    }
    CHECK(f.find_text("take 404") != nullptr);
    CHECK(f.model.follow_latest);
}

TEST_CASE("dashboard trace: clicking a row selects its decision and stops following")
{
    ViewFixture f("trace");
    feed_numbered(f.feed, 20);
    f.settle();
    f.frames();
    const LayoutNode* row = f.find_text("take 10");
    REQUIRE(row != nullptr);
    f.driver.click(rect_centre(row->rect), f.column_of([&f] { f.view->draw(f.model); }));
    CHECK_FALSE(f.model.follow_latest);
    const RecordView selected = selected_view(f.model);
    REQUIRE(selected.valid);
    CHECK(selected.record->chosen == 10);
}

TEST_CASE("dashboard trace: scrolling away from the end pauses following and the toggle resumes it")
{
    ViewFixture f("trace");
    feed_numbered(f.feed, 200);
    f.settle();
    f.frames();
    REQUIRE(f.model.follow_latest);
    f.driver.move_to({ 100.0f, 200.0f });
    f.driver.wheel({ 0.0f, 3.0f });
    f.frames(3);
    CHECK_FALSE(f.model.follow_latest);
    const LayoutNode* toggle = f.find_text("Follow latest");
    REQUIRE(toggle != nullptr);
    f.driver.click(rect_centre(toggle->rect), f.column_of([&f] { f.view->draw(f.model); }));
    CHECK(f.model.follow_latest);
}

TEST_CASE("dashboard trace: arrow keys move the selection only while the panel has the keyboard")
{
    ViewFixture f("trace");
    feed_numbered(f.feed, 30);
    f.settle();
    f.frames();
    const uint64_t latest = f.feed.view(f.feed.size() - 1).record->sequence;

    f.driver.key_press(ImKey::Up);
    f.frames(1);
    CHECK(f.model.follow_latest);

    f.model.keyboard_focus = true;
    f.driver.key_press(ImKey::Up);
    f.frames(1);
    CHECK_FALSE(f.model.follow_latest);
    CHECK(f.model.selected_sequence == latest - 1);

    f.driver.key_press(ImKey::Home);
    f.frames(1);
    CHECK(f.model.selected_sequence == f.feed.view(0).record->sequence);

    f.driver.key_press(ImKey::Down);
    f.frames(1);
    CHECK(f.model.selected_sequence == f.feed.view(0).record->sequence + 1);

    f.driver.key_press(ImKey::End);
    f.frames(1);
    CHECK(f.model.follow_latest);
}

TEST_CASE("dashboard stats: keys are grouped by namespace and show the feed's aggregates")
{
    ViewFixture f("stats");
    DummyState state(10);
    for (double nodes : { 10.0, 30.0 })
    {
        Decision decision;
        decision.chosen = 1;
        add_metric(decision.extra, "search/nodes", nodes);
        add_metric(decision.extra, "eval/score", 0.5);
        add_metric(decision.extra, "loose", 7.0);
        f.feed.on_decision(state, decision);
    }
    f.settle();
    f.frames();
    CHECK(f.find_text("search") != nullptr);
    CHECK(f.find_text("eval") != nullptr);
    CHECK(f.find_text("other") != nullptr);
    CHECK(f.find_text("nodes") != nullptr);
    CHECK(f.find_text("20") != nullptr);
    CHECK(f.find_text("0.5") != nullptr);
    CHECK(f.find_text("Dropped") == nullptr);
}

TEST_CASE("dashboard stats: the Ply column follows the selected decision")
{
    ViewFixture f("stats");
    DummyState state(10);
    for (double nodes : { 111.0, 222.0 })
    {
        Decision decision;
        decision.chosen = 1;
        add_metric(decision.extra, "search/nodes", nodes);
        f.feed.on_decision(state, decision);
    }
    f.settle();
    f.frames();
    CHECK(f.find_text("222") != nullptr);
    select_record(f.model, f.feed.view(0).record->sequence);
    f.frames();
    CHECK(f.find_text("111") != nullptr);
}

TEST_CASE("dashboard stats: overflowing keys are reported and an empty feed says so")
{
    FeedOptions options;
    options.max_keys = 1;
    ViewFixture f("stats", options);
    f.frames();
    CHECK(f.find_text("No diagnostics reported") != nullptr);
    DummyState state(10);
    Decision decision;
    decision.chosen = 1;
    add_metric(decision.extra, "a/one", 1.0);
    add_metric(decision.extra, "a/two", 2.0);
    f.feed.on_decision(state, decision);
    f.settle();
    f.frames();
    CHECK(count_text_prefix(f.context, "Dropped:") == 1);
}

TEST_CASE("dashboard views: shapes no current game produces still draw")
{
    for (const char* id : { "trace", "stats", "probabilities", "values", "decision" })
    {
        INFO(id);
        FeedOptions options;
        options.max_scores = 8;
        options.max_keys = 128;
        options.max_metrics = 128;
        ViewFixture f(id, options);
        DummyState state(600);
        for (int32_t i = 0; i < 6; ++i)
        {
            Decision decision;
            decision.player = static_cast<PlayerId>(i);
            decision.chosen = static_cast<ActionId>(i);
            for (int32_t a = 0; a < 500; ++a)
            {
                set_value(decision, static_cast<ActionId>(a), a == i ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN());
            }
            for (int32_t k = 0; k < 120; ++k)
            {
                add_metric(decision.extra, "ns" + std::to_string(k % 20) + "/key" + std::to_string(k), static_cast<double>(k) * 1.5);
            }
            f.feed.on_decision(state, decision);
        }
        CHECK_NOTHROW(f.settle());
        CHECK_NOTHROW(f.frames());
    }
}

TEST_CASE("dashboard views: every registered game against every registered strategy draws the trace and stats")
{
    for (const std::string& game_id : GameRegistry::names())
    {
        UniquePtr<IGame> game = GameRegistry::create(game_id);
        if (game == nullptr)
        {
            continue;
        }
        for (const std::string& strategy_id : StrategyRegistry::names())
        {
            INFO(game_id << " / " << strategy_id);
            SmallVector<UniquePtr<IStrategy>, 2> owned;
            SmallVector<IStrategy*, 2> seats;
            bool usable = true;
            for (int32_t seat = 0; seat < game->num_players() && usable; ++seat)
            {
                try
                {
                    owned.push_back(StrategyRegistry::create(strategy_id, {}));
                }
                catch (const Error&)
                {
                    usable = false;
                }
                usable = usable && owned.back() != nullptr;
                seats.push_back(usable ? owned.back().get() : nullptr);
            }
            if (!usable)
            {
                continue;
            }
            Match match(*game, seats);
            if (!Match::missing_capabilities(*seats[0], Match::build_context(*game, match.state())).empty())
            {
                continue;
            }
            ViewFixture f("trace");
            UniquePtr<IDashboardView> trace = std::move(f.view);
            UniquePtr<IDashboardView> stats = DashboardViewRegistry::create("stats");
            UniquePtr<IDashboardView> decision_view = DashboardViewRegistry::create("decision");
            f.feed.begin_match();
            match.set_observer(&f.feed);
            for (int32_t step = 0; step < 12 && !match.is_terminal(); ++step)
            {
                try
                {
                    match.apply(match.decide());
                }
                catch (const Error&)
                {
                    break;
                }
                f.view = std::move(trace);
                CHECK_NOTHROW(f.frames(1));
                trace = std::move(f.view);
                f.view = std::move(stats);
                CHECK_NOTHROW(f.frames(1));
                stats = std::move(f.view);
                f.view = std::move(decision_view);
                CHECK_NOTHROW(f.frames(1));
                decision_view = std::move(f.view);
            }
            match.set_observer(nullptr);
        }
    }
}

namespace
{

void add_scored(DashboardFeed& feed, ActionId chosen, std::initializer_list<std::tuple<ActionId, double, double>> scores, bool probabilities, bool values)
{
    DummyState state(10);
    Decision decision;
    decision.chosen = chosen;
    for (const auto& [action, probability, value] : scores)
    {
        if (probabilities)
        {
            set_probability(decision, action, probability);
        }
        if (values)
        {
            set_value(decision, action, value);
        }
    }
    feed.on_decision(state, decision);
}

size_t index_of(const std::vector<std::string>& texts, std::string_view text)
{
    return static_cast<size_t>(std::find(texts.begin(), texts.end(), text) - texts.begin());
}

} // namespace

TEST_CASE("dashboard decision: rows are sorted best first and the chosen row is accented")
{
    ViewFixture f("decision");
    add_scored(f.feed, 1, { { 1, 0.1, 0.0 }, { 2, 0.6, 0.0 }, { 3, 0.3, 0.0 } }, true, false);
    f.settle();
    f.frames();
    const std::vector<std::string> texts = f.painted_texts();
    CHECK(index_of(texts, "take 2") < index_of(texts, "take 3"));
    CHECK(index_of(texts, "take 3") < index_of(texts, "take 1"));
    CHECK(f.painted("60.0%"));
    CHECK(f.painted("Player 1 chose take 1"));
    const std::vector<RectCmd> accents = f.painted_rects(f.context.gui_theme().base.accent);
    CHECK_FALSE(accents.empty());
}

TEST_CASE("dashboard decision: value bars grow from the middle, blue upward and orange downward")
{
    ViewFixture f("decision");
    add_scored(f.feed, 1, { { 1, 0.0, 1.0 }, { 2, 0.0, -0.5 }, { 3, 0.0, 0.0 } }, false, true);
    f.settle();
    f.frames();
    const GuiTheme& theme = f.context.gui_theme();
    const std::vector<RectCmd> positive = f.painted_rects(theme.palette[0]);
    const std::vector<RectCmd> negative = f.painted_rects(theme.palette[1]);
    REQUIRE(positive.size() == 1);
    REQUIRE(negative.size() == 1);
    const std::vector<RectCmd> ticks = f.painted_rects(theme.base.text);
    REQUIRE(ticks.size() >= 3);
    const float centre = ticks[0].rect.min[0] + ticks[0].rect.size[0] * 0.5f;
    CHECK(positive[0].rect.min[0] == doctest::Approx(centre));
    CHECK(negative[0].rect.min[0] + negative[0].rect.size[0] == doctest::Approx(centre));
    CHECK(negative[0].rect.size[0] == doctest::Approx(positive[0].rect.size[0] * 0.5f).epsilon(0.05));
    CHECK(f.painted("+1.00"));
    CHECK(f.painted("-0.50"));
    CHECK(f.painted("+0.00"));
}

TEST_CASE("dashboard decision: a column the strategy never fills is left out")
{
    {
        ViewFixture f("decision");
        add_scored(f.feed, 1, { { 1, 0.0, 1.0 }, { 2, 0.0, -1.0 } }, false, true);
        f.settle();
        f.frames();
        CHECK(f.painted("Value"));
        CHECK_FALSE(f.painted("Probability"));
    }
    {
        ViewFixture f("decision");
        add_scored(f.feed, 1, { { 1, 0.5, 0.0 }, { 2, 0.5, 0.0 } }, true, false);
        f.settle();
        f.frames();
        CHECK(f.painted("Probability"));
        CHECK_FALSE(f.painted("Value"));
    }
    {
        ViewFixture f("decision");
        add_scored(f.feed, 1, { { 1, 0.5, 1.0 }, { 2, 0.5, -1.0 } }, true, true);
        f.settle();
        f.frames();
        CHECK(f.painted("Probability"));
        CHECK(f.painted("Value"));
    }
}

TEST_CASE("dashboard decision: a label that fits its column is never cut at the default width")
{
    ViewFixture f("decision");
    feed_uniform(f.feed, "hexapawn", 0);
    f.settle();
    f.frames();
    const RecordView record = f.feed.view(0);
    for (uint32_t i = 0; i < record.record->score_count; ++i)
    {
        CHECK(f.painted(label_view(record.scores[i].label)));
    }
}

TEST_CASE("dashboard decision: hovering a row sets the linked action and a click pins it")
{
    ViewFixture f("decision");
    add_scored(f.feed, 1, { { 1, 0.6, 0.0 }, { 2, 0.4, 0.0 } }, true, false);
    f.settle();
    f.frames();
    CHECK(f.model.hovered_action == INVALID_ACTION);
    const LayoutNode* rows = f.find_named("decision-rows");
    REQUIRE(rows != nullptr);
    const Vec2f first_row = { rows->rect.min[0] + 40.0f, rows->rect.min[1] + 22.0f + 11.0f };
    f.driver.click(first_row, f.column_of([&f] { f.view->draw(f.model); }));
    CHECK(f.model.pinned_action == 1);
    f.driver.click(first_row, f.column_of([&f] { f.view->draw(f.model); }));
    CHECK(f.model.pinned_action == INVALID_ACTION);
}

TEST_CASE("dashboard decision: the value history waits for a second record")
{
    ViewFixture f("decision");
    add_scored(f.feed, 1, { { 1, 0.0, 1.0 } }, false, true);
    f.settle();
    f.frames();
    CHECK(f.find_named("chosen value") == nullptr);
    add_scored(f.feed, 1, { { 1, 0.0, 0.5 } }, false, true);
    f.settle();
    f.frames();
    CHECK(f.find_named("chosen value") != nullptr);
    CHECK(f.find_text("Chosen value by decision") != nullptr);
}

TEST_CASE("dashboard stats: whole numbers group thousands and a lone sample drops the repeated columns")
{
    ViewFixture f("stats");
    DummyState state(10);
    Decision decision;
    decision.chosen = 1;
    add_metric(decision.extra, "search/nodes", 549945.0);
    f.feed.on_decision(state, decision);
    f.settle();
    f.frames();
    CHECK(f.find_text("549,945") != nullptr);
    CHECK(f.find_text("nodes") != nullptr);
    CHECK(f.find_text("Mean") == nullptr);
    CHECK(f.find_text("Count") == nullptr);

    f.feed.on_decision(state, decision);
    f.settle();
    f.frames();
    CHECK(f.find_text("Mean") != nullptr);
    CHECK(f.find_text("Count") != nullptr);
}

TEST_CASE("dashboard trace: players read from 1 and the probability column appears only when a record has one")
{
    ViewFixture f("trace");
    DummyState state(10);
    Decision decision;
    decision.chosen = 1;
    decision.player = 1;
    set_value(decision, 1, 0.5);
    f.feed.on_decision(state, decision);
    f.settle();
    f.frames();
    CHECK(f.find_text("Player 2") != nullptr);
    CHECK(f.find_text("Prob") == nullptr);

    set_probability(decision, 1, 0.9);
    f.feed.on_decision(state, decision);
    f.settle();
    f.frames();
    CHECK(f.find_text("Prob") != nullptr);
}

TEST_CASE("dashboard trace: one line names the match and counts its decisions in the singular and plural")
{
    ViewFixture f("trace");
    feed_uniform(f.feed, "tictactoe", 0);
    f.settle();
    f.frames();
    CHECK(f.find_text("Match 1 \xC2\xB7 1 decision") != nullptr);
    feed_uniform(f.feed, "tictactoe", 1);
    f.settle();
    f.frames();
    CHECK(f.find_text("Match 1 \xC2\xB7 2 decisions") != nullptr);
    select_record(f.model, f.feed.view(0).record->sequence);
    f.frames();
    CHECK(f.find_text("Match 1 \xC2\xB7 2 decisions \xC2\xB7 viewing 1 of 2") != nullptr);
}
