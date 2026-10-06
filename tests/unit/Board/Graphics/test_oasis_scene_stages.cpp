#include "doctest.h"

#include "Oasis/Game/HexapawnGame.h"
#include "Oasis/Game/HexapawnPresenter.h"
#include "Oasis/Game/TicTacToeGame.h"
#include "Oasis/Game/TicTacToePresenter.h"
#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"

using namespace oryx;
using namespace oryx::test;
using namespace oasis;

namespace
{

struct RendererGuard
{
    RendererGuard() { Renderer::init({ RHIBackend::Null }); }
    ~RendererGuard() { Renderer::shutdown(); }
};

// Samples how much has been batched when the Overlay stage starts, i.e. after the board (submitted first) drew in Scene2D.
class StageProbe final : public RenderSource
{
public:
    uint32_t at_overlay = 0;

    void render_stage(RenderStage stage, StageContext& context) override
    {
        static_cast<void>(context);
        if (stage == RenderStage::Overlay)
        {
            at_overlay = Renderer::batch_stats().primitives;
        }
    }
};

void check_game_uses_both_stages(PresentedGraphicsBoard2D& board, IState& state)
{
    board.on_turn(state);
    BoardInput input;
    input.viewport = { 800.0f, 600.0f };
    input.cursor = { 400.0f, 300.0f };

    const Camera2D camera = Camera2D::screen_space(800.0f, 600.0f);
    SceneRenderer& scene = Renderer::scene();
    StageProbe probe;
    scene.begin_scene(view_over(camera, 800.0f, 600.0f));
    board.render(input);
    scene.submit(probe);
    scene.end_scene();

    CHECK(probe.at_overlay > 0);
    CHECK(Renderer::batch_stats().primitives > probe.at_overlay);
    CHECK_FALSE(scene.open());
    CHECK_FALSE(scene.batcher_2d().open());
    Renderer::end_frame();
}

} // namespace

TEST_CASE("Oasis games draw their board in Scene2D and their status text in Overlay")
{
    RendererGuard guard;
    {
        PresentedGraphicsBoard2D board(create_unique<TicTacToePresenter>(), "tictactoe", k_all_seats);
        TicTacToeState state;
        check_game_uses_both_stages(board, state);
    }
    {
        PresentedGraphicsBoard2D board(create_unique<HexapawnPresenter>(), "hexapawn", k_all_seats);
        HexapawnState state;
        check_game_uses_both_stages(board, state);
    }
}
