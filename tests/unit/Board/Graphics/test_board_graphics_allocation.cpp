#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"
#include "unit/MemoryTestSupport.h"

#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr Vec2f k_viewport = { 800.0f, 600.0f };

struct RendererGuard
{
    RendererGuard() { Renderer::init({ RHIBackend::Null }); }
    ~RendererGuard() { Renderer::shutdown(); }
};

BoardInput input_over(const BoardScene& scene, SpaceId space)
{
    BoardProjection2D layout = fit_board_2d(scene, k_viewport);
    Vec2f world = board_to_world(layout, scene.layout->position(space));
    BoardInput input;
    input.viewport = k_viewport;
    input.cursor = { world[0], k_viewport[1] - world[1] };
    return input;
}

// A pile with more legal moves than ActionList holds inline.
class WideState : public IState
{
public:
    ActionList legal_actions() const override
    {
        ActionList actions;
        for (ActionId action = 1; action <= 20; ++action)
        {
            actions.push_back(action);
        }
        return actions;
    }

    void apply(ActionId) override {}
    void undo(ActionId) override {}
    PlayerId current_player() const override { return 0; }
    bool is_terminal() const override { return false; }
    Outcome outcome() const override { return {}; }
    std::string action_to_string(ActionId action) const override { return "take " + to_string(action); }
};

} // namespace

TEST_CASE("A windowed board frame over an unchanged state allocates nothing once warm")
{
    RendererGuard renderer;
    PresentedGraphicsBoard2D board(create_unique<Grid19Presenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board.on_turn(*state);

    BoardScene scene;
    board.presentation().build_scene(k_no_space, scene);
    REQUIRE(scene.layout->space_count() == 361);

    BoardProjection2D layout = fit_board_2d(scene, k_viewport);
    OptionButton2D button = option_button_2d(layout, scene.options.size(), 0);
    BoardInput option_click;
    option_click.viewport = k_viewport;
    option_click.cursor = { button.centre[0], k_viewport[1] - button.centre[1] };
    option_click.select = true;

    auto run_frame = [&](uint32_t frame)
    {
        BoardInput hover = input_over(scene, frame % 361);
        board.update(hover, 0.016);
        BoardInput press = hover;
        press.select = true;
        press.select_down = true;
        board.update(press, 0.016);
        BoardInput release = hover;
        release.select_released = true;
        board.update(release, 0.016);
        board.update(option_click, 0.016);
        board.poll_action(*state);
    };

    for (uint32_t warm = 0; warm < 5; ++warm)
    {
        run_frame(warm);
    }

    MemoryStats before = all_allocations();
    for (uint32_t frame = 0; frame < 100; ++frame)
    {
        run_frame(frame);
    }
    MemoryStats delta = memory_delta(before, all_allocations());

    CHECK(delta.allocation_count == 0);

    // The board itself allocates nothing in render; the few that remain are inside the Renderer, so only per-frame growth is ruled out.
    for (uint32_t warm = 0; warm < 5; ++warm)
    {
        render_board(board, input_over(scene, warm));
    }
    before = all_allocations();
    for (uint32_t frame = 0; frame < 100; ++frame)
    {
        render_board(board, input_over(scene, frame % 361));
    }
    delta = memory_delta(before, all_allocations());
    CHECK(delta.allocation_count < 100);
}

TEST_CASE("A board over a state with more than ActionList's inline capacity allocates in legal_actions each frame, and nowhere else")
{
    BoardPresentation presentation(create_unique<FakePresenter>(), "wide", k_all_seats);
    WideState state;
    BoardScene scene;
    for (uint32_t warm = 0; warm < 3; ++warm)
    {
        presentation.update(state);
        presentation.build_scene(k_no_space, scene);
    }

    MemoryStats before = all_allocations();
    for (uint32_t frame = 0; frame < 100; ++frame)
    {
        presentation.update(state);
        presentation.build_scene(k_no_space, scene);
    }
    MemoryStats delta = memory_delta(before, all_allocations());

    CHECK(delta.allocation_count <= 2 * 100);
}
