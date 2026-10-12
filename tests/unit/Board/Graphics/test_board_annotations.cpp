#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToePresenter.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

constexpr Vec2f k_viewport = { 800.0f, 600.0f };

SharedPtr<const IBoardPresenter> shared_presenter(UniquePtr<IBoardPresenter> presenter)
{
    return SharedPtr<const IBoardPresenter>(std::move(presenter));
}

class ListDescriber final : public IActionDescriber
{
public:
    bool describe(const IState&, ActionId action, ActionDescription& out) const override
    {
        set_label(out.label, "m" + std::to_string(action));
        const std::vector<uint32_t>& path = paths.at(action);
        out.space_count = static_cast<uint8_t>(path.size());
        std::copy(path.begin(), path.end(), out.spaces);
        return out.space_count > 0;
    }

    std::map<ActionId, std::vector<uint32_t>> paths;
};

struct DecisionFixture
{
    DashboardFeed feed;
    SharedPtr<ListDescriber> describer = create_shared<ListDescriber>();
    TicTacToeState state;

    DecisionFixture() { feed.set_describer(describer); }

    void add(ActionId chosen, std::initializer_list<std::tuple<ActionId, double, double>> scores, bool probabilities, bool values)
    {
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
};

const BoardAnnotation* find_item(const BoardAnnotations& annotations, AnnotationKind kind, SpaceId destination_space)
{
    for (const BoardAnnotation& item : annotations.items)
    {
        if (item.kind == kind && destination(item) == destination_space)
        {
            return &item;
        }
    }
    return nullptr;
}

struct BoardDrawFixture
{
    UniquePtr<RendererContext> renderer = create_renderer_context({ RHIBackend::Null });
    FakeFontSource* source = nullptr;
    Font font = make_fake_font(source);
    PresentedGraphicsBoard2D board{ create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats };
    HexapawnState state;
    BoardScene scene;
    BoardProjection2D projection;

    BoardDrawFixture()
    {
        board.on_turn(state);
        board.presentation().build_scene(k_no_space, scene);
        projection = fit_board_2d(scene, k_viewport);
    }

    ~BoardDrawFixture() { renderer->rhi->wait_idle(); }

    uint32_t primitives(const BoardAnnotationLayer& layer)
    {
        std::vector<DrawItem> sink;
        BatchRenderer2D batcher{ batch_renderer_desc(*renderer, sink) };
        batcher.begin(Camera2D::screen_space(k_viewport[0], k_viewport[1]));
        draw_board_2d(batcher, scene, projection, BoardTheme2D{}, font, layer);
        batcher.end();
        return batcher.stats().primitives;
    }
};

BoardAnnotation item_of(AnnotationKind kind, std::initializer_list<uint32_t> path, float weight = 1.0f, float value = std::numeric_limits<float>::quiet_NaN())
{
    BoardAnnotation item;
    item.kind = kind;
    item.path_count = static_cast<uint8_t>(path.size());
    std::copy(path.begin(), path.end(), item.path);
    item.weight = weight;
    item.value = value;
    return item;
}

} // namespace

TEST_CASE("PresenterActionDescriber names a Tic-Tac-Toe action by its board space")
{
    PresenterActionDescriber describer(shared_presenter(create_unique<TicTacToePresenter>()));
    TicTacToeState state;
    ActionDescription out;
    REQUIRE(describer.describe(state, 4, out));
    CHECK(out.space_count == 1);
    CHECK(out.spaces[0] == 4);
    const SharedPtr<const BoardLayout> layout = TicTacToePresenter().layout(state);
    CHECK(label_view(out.label) == layout->label(4));
    CHECK(label_view(out.label) != state.action_to_string(4));
}

TEST_CASE("PresenterActionDescriber joins a Hexapawn move's spaces and reports its destination last")
{
    PresenterActionDescriber describer(shared_presenter(create_unique<HexapawnPresenter>()));
    HexapawnState state;
    const ActionId forward = HexapawnState::action_for(6, HexapawnState::Forward);
    ActionDescription out;
    REQUIRE(describer.describe(state, forward, out));
    REQUIRE(out.space_count == 2);
    CHECK(out.spaces[0] == 6);
    CHECK(out.spaces[1] == 3);
    const SharedPtr<const BoardLayout> layout = HexapawnPresenter().layout(state);
    CHECK(label_view(out.label) == layout->label(6) + "-" + layout->label(3));
}

TEST_CASE("make_presenter_describer finds a game's presenter and is null for an unknown game")
{
    CHECK(make_presenter_describer("tictactoe") != nullptr);
    CHECK(make_presenter_describer("no such game") == nullptr);
}

TEST_CASE("PresenterActionDescriber allocates nothing once warm")
{
    if (!MemoryTracker::is_installed())
    {
        return;
    }
    PresenterActionDescriber describer(shared_presenter(create_unique<HexapawnPresenter>()));
    HexapawnState state;
    const ActionId forward = HexapawnState::action_for(6, HexapawnState::Forward);
    ActionDescription out;
    REQUIRE(describer.describe(state, forward, out));
    const MemoryStats before = all_allocations();
    for (int32_t i = 0; i < 100; ++i)
    {
        std::ignore = describer.describe(state, forward, out);
    }
    CHECK(memory_delta(before, all_allocations()).allocation_count == 0);
}

TEST_CASE("annotate_decision adds the chosen path and sums probability into one heat item per destination")
{
    DecisionFixture f;
    f.describer->paths = { { 1, { 0, 3 } }, { 2, { 1, 3 } }, { 3, { 2, 4 } } };
    f.add(2, { { 1, 0.3, 0.0 }, { 2, 0.5, 0.0 }, { 3, 0.2, 0.0 } }, true, false);
    BoardAnnotations annotations;
    annotate_decision(f.feed.view(0), annotations);

    const BoardAnnotation* chosen = find_item(annotations, AnnotationKind::Chosen, 3);
    REQUIRE(chosen != nullptr);
    CHECK(chosen->path_count == 2);
    CHECK(chosen->path[0] == 1);
    CHECK(chosen->source == k_dashboard_annotation_source);

    const BoardAnnotation* shared = find_item(annotations, AnnotationKind::Heat, 3);
    const BoardAnnotation* other = find_item(annotations, AnnotationKind::Heat, 4);
    REQUIRE((shared != nullptr && other != nullptr));
    CHECK(shared->value == doctest::Approx(0.8f));
    CHECK(shared->weight == doctest::Approx(1.0f));
    CHECK(other->weight == doctest::Approx(0.25f));
    CHECK(std::count_if(annotations.items.begin(), annotations.items.end(), [](const BoardAnnotation& item) { return item.kind == AnnotationKind::Heat; }) == 2);
}

TEST_CASE("annotate_decision heats a value-only strategy by relative value and prints no percentage")
{
    DecisionFixture f;
    f.describer->paths = { { 1, { 0 } }, { 2, { 1 } }, { 3, { 2 } } };
    f.add(3, { { 1, 0.0, -1.0 }, { 2, 0.0, 0.0 }, { 3, 0.0, 1.0 } }, false, true);
    BoardAnnotations annotations;
    annotate_decision(f.feed.view(0), annotations);

    CHECK(find_item(annotations, AnnotationKind::Heat, 0)->weight == doctest::Approx(0.0f));
    CHECK(find_item(annotations, AnnotationKind::Heat, 1)->weight == doctest::Approx(0.5f));
    CHECK(find_item(annotations, AnnotationKind::Heat, 2)->weight == doctest::Approx(1.0f));
    CHECK(std::isnan(find_item(annotations, AnnotationKind::Heat, 2)->value));
    CHECK(find_item(annotations, AnnotationKind::Value, 0)->value == doctest::Approx(-1.0f));
    CHECK(find_item(annotations, AnnotationKind::Value, 0)->weight == doctest::Approx(-1.0f));
}

TEST_CASE("annotate_decision replaces only its own source and clears on an invalid view")
{
    DecisionFixture f;
    f.describer->paths = { { 1, { 0 } }, { 2, { 1 } } };
    f.add(1, { { 1, 0.6, 0.0 }, { 2, 0.4, 0.0 } }, true, false);
    BoardAnnotations annotations;
    BoardAnnotation hint = item_of(AnnotationKind::Heat, { 7 });
    hint.source = 9;
    add_annotation(annotations, hint);

    annotate_decision(f.feed.view(0), annotations);
    const size_t with_decision = annotations.items.size();
    annotate_decision(f.feed.view(0), annotations);
    CHECK(annotations.items.size() == with_decision);
    annotate_decision(RecordView{}, annotations);
    REQUIRE(annotations.items.size() == 1);
    CHECK(annotations.items[0].source == 9);
}

TEST_CASE("annotate_decision skips actions without spaces and the destination lookups agree")
{
    DecisionFixture f;
    f.describer->paths = { { 1, { 0, 5 } }, { 2, {} } };
    f.add(1, { { 1, 0.7, 0.0 }, { 2, 0.3, 0.0 } }, true, false);
    const RecordView view = f.feed.view(0);
    BoardAnnotations annotations;
    annotate_decision(view, annotations);

    CHECK(find_item(annotations, AnnotationKind::Heat, 5) != nullptr);
    CHECK(std::count_if(annotations.items.begin(), annotations.items.end(), [](const BoardAnnotation& item) { return item.kind == AnnotationKind::Heat; }) == 1);
    CHECK(action_destination(view, 1) == 5);
    CHECK(action_destination(view, 2) == k_no_space);
    CHECK(action_destination(view, 77) == k_no_space);
    CHECK(action_ending_at(view, 5) == 1);
    CHECK(action_ending_at(view, 4) == INVALID_ACTION);
}

TEST_CASE("draw_board_2d draws nothing extra with no annotations or every flag off")
{
    BoardDrawFixture f;
    const uint32_t plain = f.primitives({});
    BoardAnnotations annotations;
    add_annotation(annotations, item_of(AnnotationKind::Heat, { 4 }));
    add_annotation(annotations, item_of(AnnotationKind::Chosen, { 6, 3 }));
    add_annotation(annotations, item_of(AnnotationKind::Value, { 3 }, 1.0f, 0.5f));
    CHECK(f.primitives({ &annotations, {}, 3, k_no_space }) == plain);
}

TEST_CASE("draw_board_2d adds one tile tint per heat item and skips spaces outside the layout")
{
    BoardDrawFixture f;
    const uint32_t plain = f.primitives({});
    BoardAnnotations annotations;
    add_annotation(annotations, item_of(AnnotationKind::Heat, { 4 }));
    add_annotation(annotations, item_of(AnnotationKind::Heat, { 5 }, 0.2f));
    CHECK(f.primitives({ &annotations, { true, false, false }, k_no_space, k_no_space }) == plain + 2);
    add_annotation(annotations, item_of(AnnotationKind::Heat, { 999 }));
    CHECK(f.primitives({ &annotations, { true, false, false }, k_no_space, k_no_space }) == plain + 2);
}

TEST_CASE("draw_board_2d puts a ring on a chosen space and an arrow along a longer path")
{
    BoardDrawFixture f;
    const uint32_t plain = f.primitives({});
    BoardAnnotations ring;
    add_annotation(ring, item_of(AnnotationKind::Chosen, { 3 }));
    const uint32_t ring_only = f.primitives({ &ring, { false, true, false }, k_no_space, k_no_space }) - plain;
    CHECK(ring_only == 1);

    BoardAnnotations move;
    add_annotation(move, item_of(AnnotationKind::Chosen, { 6, 3 }));
    CHECK(f.primitives({ &move, { false, true, false }, k_no_space, k_no_space }) - plain == ring_only + 2);

    BoardAnnotations jump;
    add_annotation(jump, item_of(AnnotationKind::Chosen, { 6, 3, 0 }));
    CHECK(f.primitives({ &jump, { false, true, false }, k_no_space, k_no_space }) - plain == ring_only + 3);

    BoardAnnotations outside;
    add_annotation(outside, item_of(AnnotationKind::Chosen, { 500 }));
    CHECK(f.primitives({ &outside, { false, true, false }, k_no_space, k_no_space }) == plain);
}

TEST_CASE("draw_board_2d shows a value chip only on the hovered annotated space")
{
    BoardDrawFixture f;
    BoardAnnotations annotations;
    add_annotation(annotations, item_of(AnnotationKind::Value, { 3 }, 1.0f, 0.5f));
    const AnnotationFlags values = { false, false, true };
    const uint32_t plain = f.primitives({});
    CHECK(f.primitives({ &annotations, values, k_no_space, k_no_space }) == plain);
    CHECK(f.primitives({ &annotations, values, 4, k_no_space }) == plain);
    CHECK(f.primitives({ &annotations, values, 3, k_no_space }) > plain);
    CHECK(f.primitives({ &annotations, {}, 3, k_no_space }) == plain);
}

TEST_CASE("draw_board_2d outlines a linked space whatever the flags")
{
    BoardDrawFixture f;
    BoardAnnotations annotations;
    const uint32_t plain = f.primitives({});
    CHECK(f.primitives({ &annotations, {}, k_no_space, 4 }) == plain + 4);
    CHECK(f.primitives({ &annotations, {}, k_no_space, 999 }) == plain);
}

TEST_CASE("PresentedGraphicsBoard2D shows the annotation chips and reports a click on one")
{
    PresentedGraphicsBoard2D board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
    TicTacToeState state;
    board.on_turn(state);
    BoardAnnotationRequest request;
    request.controls = true;
    request.flags = { false, true, false };
    request.legend = "Ply 1";
    board.set_annotations(request);

    BoardInput input;
    input.viewport = k_viewport;
    input.cursor = { 5.0f, 5.0f };
    board.update(input, 0.016);
    board.update(input, 0.016);
    const LayoutTree& layout = board.ui().layout();
    const LayoutNode* heat = nullptr;
    for (uint32_t index = 0; index < layout.node_count(); ++index)
    {
        if (layout.node(index).paint.text == "Heat")
        {
            heat = &layout.node(index);
        }
    }
    REQUIRE(heat != nullptr);
    CHECK_FALSE(board.annotation_flags().heat);
    CHECK(board.annotation_flags().chosen);

    input.cursor = rect_centre(heat->rect);
    board.update(input, 0.016);
    input.select = true;
    input.select_down = true;
    board.update(input, 0.016);
    input.select = false;
    input.select_down = false;
    input.select_released = true;
    board.update(input, 0.016);
    CHECK(board.annotation_flags().heat);
    CHECK(board.annotation_flags().chosen);
    CHECK(board.poll_action(state) == PENDING_ACTION);
}

TEST_CASE("PresentedGraphicsBoard2D draws no annotation controls until the application asks")
{
    PresentedGraphicsBoard2D board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
    TicTacToeState state;
    board.on_turn(state);
    BoardInput input;
    input.viewport = k_viewport;
    board.update(input, 0.016);
    const LayoutTree& layout = board.ui().layout();
    for (uint32_t index = 0; index < layout.node_count(); ++index)
    {
        CHECK(layout.node(index).paint.text != "Heat");
    }
}

TEST_CASE("every registered game against every registered strategy annotates and draws with every flag on")
{
    UniquePtr<RendererContext> renderer = create_renderer_context({ RHIBackend::Null });
    FakeFontSource* source = nullptr;
    Font font = make_fake_font(source);
    for (const std::string& game_id : BoardPresenterRegistry::names())
    {
        UniquePtr<IGame> game = GameRegistry::create(game_id);
        if (game == nullptr)
        {
            continue;
        }
        const SharedPtr<const IActionDescriber> describer = make_presenter_describer(game_id);
        REQUIRE(describer != nullptr);
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
            DashboardFeed feed;
            feed.set_describer(describer);
            feed.begin_match();
            match.set_observer(&feed);
            PresentedGraphicsBoard2D board(BoardPresenterRegistry::create(game_id), game_id, k_all_seats);
            BoardAnnotations annotations;
            for (int32_t step = 0; step < 10 && !match.is_terminal(); ++step)
            {
                try
                {
                    match.apply(match.decide());
                }
                catch (const Error&)
                {
                    break;
                }
                board.on_turn(match.state());
                BoardAnnotationRequest request;
                request.controls = true;
                request.flags = { true, true, true };
                annotate_decision(feed.view(feed.size() - 1), annotations);
                request.annotations = &annotations;
                board.set_annotations(request);
                BoardInput input;
                input.viewport = k_viewport;
                CHECK_NOTHROW(board.update(input, 0.016));

                BoardScene scene;
                board.presentation().build_scene(k_no_space, scene);
                const BoardProjection2D projection = fit_board_2d(scene, k_viewport);
                std::vector<DrawItem> sink;
                BatchRenderer2D batcher{ batch_renderer_desc(*renderer, sink) };
                batcher.begin(Camera2D::screen_space(k_viewport[0], k_viewport[1]));
                for (const BoardAnnotation& item : annotations.items)
                {
                    CHECK_NOTHROW(draw_board_2d(batcher, scene, projection, BoardTheme2D{}, font, { &annotations, { true, true, true }, destination(item), destination(item) }));
                }
                batcher.end();
            }
            match.set_observer(nullptr);
        }
    }
    renderer->rhi->wait_idle();
}
