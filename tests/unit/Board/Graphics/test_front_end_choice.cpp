#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

#include "Oryx/Core/PolledInput.h"

using namespace oryx;
using namespace oryx::test;
using namespace oryx::selection;

namespace
{

class FakeGraphicsBoard : public IGraphicsBoard
{
public:
    void on_turn(const IState&) override {}
    ActionId poll_action(const IState&) override { return PENDING_ACTION; }
    void update(const BoardInput&, double) override {}
    void render(const BoardInput&) override {}
    bool shows_moves() const override { return true; }
};

struct ExtraGame
{
    ExtraGame()
    {
        GameRegistry::register_factory("zz-extra", [](const Params&) -> UniquePtr<IGame> { return create_unique<DummyGame>(5); });
    }

    ~ExtraGame() { GameRegistry::unregister_factory("zz-extra"); }
};

struct FakeGraphicsBoardRegistration
{
    explicit FakeGraphicsBoardRegistration(std::string game)
        : m_game(std::move(game))
    {
        GraphicsBoardRegistry::register_factory(m_game, [](const Params&) -> UniquePtr<IGraphicsBoard> { return create_unique<FakeGraphicsBoard>(); });
    }

    ~FakeGraphicsBoardRegistration() { GraphicsBoardRegistry::unregister_factory(m_game); }

    std::string m_game;
};

} // namespace

TEST_CASE("choose_front_end is Console when headless or graphics are not built")
{
    FakeGraphicsBoardRegistration board("tictactoe");
    FrontEnd front_end = FrontEnd::Graphical;
    std::string game;

    CHECK(choose_front_end("", true, true, front_end, game));
    CHECK(front_end == FrontEnd::Console);

    CHECK(choose_front_end("", false, false, front_end, game));
    CHECK(front_end == FrontEnd::Console);
}

TEST_CASE("choose_front_end is Graphical only for a game with a graphics board or a presenter, and resolves the game")
{
    ExtraGame extra;
    FrontEnd front_end = FrontEnd::Graphical;
    std::string game;

    CHECK(choose_front_end("zz-extra", false, true, front_end, game));
    CHECK(front_end == FrontEnd::Console);
    CHECK(game == "zz-extra");

    {
        FakeGraphicsBoardRegistration board("zz-extra");
        CHECK(choose_front_end("zz-extra", false, true, front_end, game));
        CHECK(front_end == FrontEnd::Graphical);
    }

    BoardPresenterRegistry::register_factory("zz-extra", [](const Params&) -> UniquePtr<IBoardPresenter> { return create_unique<FakePresenter>(); });
    CHECK(choose_front_end("zz-extra", false, true, front_end, game));
    CHECK(front_end == FrontEnd::Graphical);
    BoardPresenterRegistry::unregister_factory("zz-extra");

    CHECK(choose_front_end("", false, true, front_end, game));
    CHECK(game == "tictactoe");
    CHECK_FALSE(choose_front_end("no-such-game", false, true, front_end, game));
}

TEST_CASE("An IGraphicsBoard reads one BoardInput per frame, asks to restart once, and closes the application on quit")
{
    class Recording : public FakeGraphicsBoard
    {
    public:
        void update(const BoardInput& input, double delta_time) override { last = input; dt = delta_time; ++updates; }
        void render(const BoardInput&) override { ++renders; }

        BoardInput last;
        double dt = 0.0;
        int32_t updates = 0;
        int32_t renders = 0;
    };

    Application app({ 0, nullptr });
    PolledInput input;
    Recording board;
    input.set_cursor(30.0f, 40.0f);
    input.set_mouse_button(MouseCode::Left, true);
    board.frame({ input, { 800.0f, 600.0f }, { 1600.0f, 1200.0f }, 2.0f, 0.25 });
    CHECK_FALSE(board.take_restart_request());

    input.begin_frame();
    input.set_key(KeyCode::R, true);
    board.frame({ input, { 800.0f, 600.0f }, { 1600.0f, 1200.0f }, 2.0f, 0.25 });
    CHECK(board.updates == 2);
    CHECK(board.renders == 2);
    CHECK(board.dt == 0.25);
    CHECK(board.last.scale == 2.0f);
    CHECK(board.last.viewport == Vec2f(800.0f, 600.0f));
    CHECK(board.last.cursor == Vec2f(30.0f, 40.0f));
    CHECK(board.take_restart_request());
    CHECK_FALSE(board.take_restart_request());

    input.begin_frame();
    input.set_mouse_button(MouseCode::Left, false);
    input.set_key(KeyCode::Escape, true);
    board.frame({ input, { 800.0f, 600.0f }, { 800.0f, 600.0f }, 1.0f, 0.016 });
    CHECK(board.updates == 2);
    CHECK(app.closing());
}
