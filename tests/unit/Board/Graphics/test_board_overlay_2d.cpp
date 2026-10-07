#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct OverlayFixture
{
    FakeFontSource* source = nullptr;
    Font font = make_fake_font(source);
    UiContext ui;
    BoardOverlayResult result;
    BoardScene scene;

    OverlayFixture()
    {
        UiTheme theme = board_ui_theme();
        theme.font = &font;
        ui.set_theme(theme);

        UniquePtr<IState> state = DummyGame(5).new_initial_state();
        BoardPresentation presentation(create_unique<FakePresenter>(), "dummy", k_all_seats);
        presentation.update(*state);
        presentation.build_scene(k_no_space, scene);
    }

    void run(const Vec2f& viewport, float scale = 1.0f)
    {
        BoardInput input;
        input.viewport = viewport;
        input.scale = scale;
        input.cursor = { -1.0f, -1.0f };
        run_board_overlay(ui, input, 0.016, false, scene, BoardOverlayText{ "player 1" }, result);
    }
};

} // namespace

TEST_CASE("The board overlay lays out the status, the board area and the option row")
{
    OverlayFixture fixture;
    fixture.run({ 800.0f, 600.0f });

    CHECK(dump_layout(fixture.ui) ==
          "overlay [0.00 0.00 800.00 600.00] row w=grow(1.00) h=grow(1.00)\n"
          "  board [28.00 56.00 744.00 452.00] row w=grow(1.00) h=grow(1.00)\n"
          "  player 1 [343.00 16.00 114.00 24.00] row w=fit h=fit floating text=\"player 1\"\n"
          "  options [208.00 536.00 384.00 64.00] row w=fit h=fixed(64.00) floating\n"
          "    take1 [208.00 550.00 120.00 36.00] row w=fixed(120.00) h=fixed(36.00) text=\"take1\"\n"
          "    take2 [340.00 550.00 120.00 36.00] row w=fixed(120.00) h=fixed(36.00) text=\"take2\"\n"
          "    take3 [472.00 550.00 120.00 36.00] row w=fixed(120.00) h=fixed(36.00) text=\"take3\"\n");
}

TEST_CASE("The board overlay records the status text and flat option buttons")
{
    OverlayFixture fixture;
    fixture.run({ 800.0f, 600.0f });

    CHECK(dump(fixture.ui.draw_list()) ==
          "surface 0\n"
          "channel 0\n"
          "  text (400.00 37.00) h=24.00 align=1 \"player 1\" rgba(1.00 1.00 1.00 1.00)\n"
          "  rect [208.00 550.00 120.00 36.00] rgba(0.25 0.28 0.36 1.00)\n"
          "  text (268.00 575.00) h=18.00 align=1 \"take1\" rgba(1.00 1.00 1.00 1.00)\n"
          "  rect [340.00 550.00 120.00 36.00] rgba(0.25 0.28 0.36 1.00)\n"
          "  text (400.00 575.00) h=18.00 align=1 \"take2\" rgba(1.00 1.00 1.00 1.00)\n"
          "  rect [472.00 550.00 120.00 36.00] rgba(0.25 0.28 0.36 1.00)\n"
          "  text (532.00 575.00) h=18.00 align=1 \"take3\" rgba(1.00 1.00 1.00 1.00)\n");
}

TEST_CASE("The board overlay is deterministic and follows the viewport")
{
    OverlayFixture fixture;
    fixture.run({ 800.0f, 600.0f });
    const std::string first = dump_layout(fixture.ui);
    fixture.run({ 800.0f, 600.0f });
    CHECK(dump_layout(fixture.ui) == first);

    fixture.run({ 1280.0f, 720.0f }, 2.0f);
    CHECK(fixture.result.board == board_region_2d({ 1280.0f, 720.0f }));
    CHECK(rect_centre(fixture.result.buttons[1].rect) == Vec2f(640.0f, 720.0f - k_board_menu_band * 0.5f));
}

TEST_CASE("The board overlay without options has no button row, and clicking elsewhere chooses nothing")
{
    OverlayFixture fixture;
    fixture.scene.options.clear();
    fixture.run({ 800.0f, 600.0f });
    CHECK(fixture.result.buttons.empty());
    CHECK(dump_layout(fixture.ui).find("options") == std::string::npos);
    CHECK_FALSE(fixture.result.chosen);
}

TEST_CASE("The board overlay reports a click on release, once, and a repeated label does not collide")
{
    OverlayFixture fixture;
    fixture.scene.options.push_back(fixture.scene.options[0]);
    fixture.run({ 800.0f, 600.0f });
    REQUIRE(fixture.result.buttons.size() == 4);

    BoardInput press;
    press.viewport = { 800.0f, 600.0f };
    press.cursor = rect_centre(fixture.result.buttons[3].rect);
    press.select = true;
    run_board_overlay(fixture.ui, press, 0.016, false, fixture.scene, BoardOverlayText{ "x" }, fixture.result);
    CHECK_FALSE(fixture.result.chosen);
    CHECK(fixture.result.pointer_over_ui);

    BoardInput release = press;
    release.select = false;
    release.select_released = true;
    run_board_overlay(fixture.ui, release, 0.016, false, fixture.scene, BoardOverlayText{ "x" }, fixture.result);
    CHECK(fixture.result.chosen);
    CHECK(fixture.result.option == 3);

    run_board_overlay(fixture.ui, release, 0.016, false, fixture.scene, BoardOverlayText{ "x" }, fixture.result);
    CHECK_FALSE(fixture.result.chosen);
}

TEST_CASE("The board overlay hides the pointer from the buttons while it is captured")
{
    OverlayFixture fixture;
    fixture.run({ 800.0f, 600.0f });

    BoardInput over;
    over.viewport = { 800.0f, 600.0f };
    over.cursor = rect_centre(fixture.result.buttons[0].rect);
    run_board_overlay(fixture.ui, over, 0.016, false, fixture.scene, BoardOverlayText{ "x" }, fixture.result);
    CHECK(fixture.result.pointer_over_ui);
    run_board_overlay(fixture.ui, over, 0.016, true, fixture.scene, BoardOverlayText{ "x" }, fixture.result);
    CHECK_FALSE(fixture.result.pointer_over_ui);
}

TEST_CASE("The board overlay shows a result banner with a Play again button once the game is over")
{
    OverlayFixture fixture;
    const BoardOverlayText over{ "", "", "You win!", "win", "press R to play again" };
    BoardInput idle;
    idle.viewport = { 800.0f, 600.0f };
    idle.cursor = { -1.0f, -1.0f };
    run_board_overlay(fixture.ui, idle, 0.016, false, fixture.scene, over, fixture.result);

    const std::string layout = dump_layout(fixture.ui);
    CHECK(layout.find("result") != std::string::npos);
    CHECK(layout.find("You win!") != std::string::npos);
    CHECK(layout.find("Play again") != std::string::npos);
    Rect banner;
    REQUIRE(fixture.ui.layout_rect(fixture.ui.id("result"), banner));
    CHECK(rect_centre(banner) == Vec2f(400.0f, 300.0f));
    const std::string drawn = dump(fixture.ui.draw_list());
    CHECK(drawn.find("You win!") != std::string::npos);
    CHECK(drawn.find("player 1") == std::string::npos);

    Rect again;
    REQUIRE(fixture.ui.layout_rect(make_im_id("Play again", fixture.ui.id("result")), again));
    BoardInput press = idle;
    press.cursor = rect_centre(again);
    press.select = true;
    run_board_overlay(fixture.ui, press, 0.016, false, fixture.scene, over, fixture.result);
    CHECK_FALSE(fixture.result.restart);
    BoardInput release = press;
    release.select = false;
    release.select_released = true;
    run_board_overlay(fixture.ui, release, 0.016, false, fixture.scene, over, fixture.result);
    CHECK(fixture.result.restart);
    run_board_overlay(fixture.ui, idle, 0.016, false, fixture.scene, over, fixture.result);
    CHECK_FALSE(fixture.result.restart);
}

TEST_CASE("The board overlay has no banner while the game runs")
{
    OverlayFixture fixture;
    fixture.run({ 800.0f, 600.0f });
    CHECK(dump_layout(fixture.ui).find("Play again") == std::string::npos);
    CHECK_FALSE(fixture.result.restart);
}
