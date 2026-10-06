#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"
#include "unit/Python/PythonTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

namespace
{

const char* k_pile_game =
    "class PileState(oryx.State):\n"
    "    def __init__(self):\n"
    "        self.stones = 3\n"
    "    def legal_actions(self):\n"
    "        return [1, 2, 3][:self.stones]\n"
    "    def apply(self, action):\n"
    "        self.stones -= action\n"
    "    def undo(self, action):\n"
    "        self.stones += action\n"
    "    def current_player(self):\n"
    "        return 0\n"
    "    def is_terminal(self):\n"
    "        return self.stones == 0\n"
    "    def outcome(self):\n"
    "        return [0.0, 0.0]\n"
    "class PileGame(oryx.Game, id='pile-board-game'):\n"
    "    num_players = 2\n"
    "    def new_initial_state(self):\n"
    "        return PileState()\n";

UniquePtr<IConsoleBoard> board_for(const std::string& game)
{
    UniquePtr<IConsoleBoard> board = ConsoleBoardRegistry::create(game);
    REQUIRE(board != nullptr);
    return board;
}

} // namespace

TEST_CASE("a class deriving from oryx.ConsoleBoard registers a board for its game and reads shows_moves")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("board.py", marker_prelude(marker) +
        "import oryx\n"
        "class Board(oryx.ConsoleBoard, game='py-board-test'):\n"
        "    shows_moves = True\n"
        "    def on_turn(self, state):\n"
        "        mark('turn:' + str(state.current_player()) + ':' + str(state.native) + ';')\n"
        "    def poll_action(self, state):\n"
        "        return state.legal_actions()[1]\n");

    RunningPython python;
    python.load(script);

    REQUIRE(ConsoleBoardRegistry::has("py-board-test"));
    REQUIRE(scripted_console_board_origin("py-board-test") != nullptr);
    CHECK(scripted_console_board_origin("py-board-test")->language == "python");

    UniquePtr<IConsoleBoard> board = board_for("py-board-test");
    CHECK(board->shows_moves());

    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board->on_turn(*state);
    CHECK(board->poll_action(*state) == 2);
    CHECK(read_file(marker) == "turn:0:None;");
}

TEST_CASE("a board reads game-specific fields of a scripted state through native")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("native.py", marker_prelude(marker) + "import oryx\n" + k_pile_game +
        "class PileBoard(oryx.ConsoleBoard, game='pile-board-game'):\n"
        "    def on_turn(self, state):\n"
        "        mark('stones=' + str(state.native.stones) + ';')\n");

    RunningPython python;
    python.load(script);

    UniquePtr<IGame> game = create_game("pile-board-game");
    UniquePtr<IState> state = game->new_initial_state();
    board_for("pile-board-game")->on_turn(*state);
    state->apply(2);
    board_for("pile-board-game")->on_turn(*state);
    CHECK(read_file(marker) == "stones=3;stones=1;");
}

TEST_CASE("a board that defines only on_turn plays moves with the generic console prompt")
{
    TempDir dir;
    std::filesystem::path script = dir.write("fallback.py",
        "import oryx\n"
        "class Board(oryx.ConsoleBoard, game='py-board-fallback'):\n"
        "    def on_turn(self, state):\n"
        "        pass\n");

    RunningPython python;
    python.load(script);

    ConsoleScope console("3\n");
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    CHECK(board_for("py-board-fallback")->poll_action(*state) == 3);
}

TEST_CASE("a board's poll_action may return PENDING_ACTION and UNDO_ACTION, but an illegal action throws")
{
    TempDir dir;
    std::filesystem::path script = dir.write("results.py",
        "import oryx\n"
        "answers = [oryx.board.PENDING_ACTION, oryx.board.UNDO_ACTION, 99]\n"
        "class Board(oryx.ConsoleBoard, game='py-board-results'):\n"
        "    def poll_action(self, state):\n"
        "        return answers.pop(0)\n");

    RunningPython python;
    python.load(script);

    UniquePtr<IConsoleBoard> board = board_for("py-board-results");
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    CHECK(board->poll_action(*state) == PENDING_ACTION);
    CHECK(board->poll_action(*state) == UNDO_ACTION);
    CHECK_THROWS_AS(board->poll_action(*state), Error);
}

TEST_CASE("a state lent to a board is read-only and only valid during the call")
{
    TempDir dir;
    std::filesystem::path marker = dir.path() / "marker.txt";
    std::filesystem::path script = dir.write("lease.py", marker_prelude(marker) +
        "import oryx\n"
        "kept = []\n"
        "class Board(oryx.ConsoleBoard, game='py-board-lease'):\n"
        "    def on_turn(self, state):\n"
        "        try:\n"
        "            state.apply(1)\n"
        "        except Exception as e:\n"
        "            mark('readonly;')\n"
        "        if kept:\n"
        "            try:\n"
        "                kept[0].is_terminal()\n"
        "            except Exception as e:\n"
        "                mark('expired;')\n"
        "        kept.append(state)\n");

    RunningPython python;
    python.load(script);

    UniquePtr<IConsoleBoard> board = board_for("py-board-lease");
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    board->on_turn(*state);
    board->on_turn(*state);
    CHECK(read_file(marker) == "readonly;readonly;expired;");
}

TEST_CASE("a board exception reaches the caller as an oryx::Error")
{
    TempDir dir;
    std::filesystem::path script = dir.write("failing.py",
        "import oryx\n"
        "class Board(oryx.ConsoleBoard, game='py-board-failing'):\n"
        "    def poll_action(self, state):\n"
        "        raise RuntimeError('no moves today')\n");

    RunningPython python;
    python.load(script);

    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    CHECK_THROWS_AS(board_for("py-board-failing")->poll_action(*state), Error);
}

TEST_CASE("a board that defines neither on_turn nor poll_action cannot be registered")
{
    TempDir dir;
    std::filesystem::path script = dir.write("empty.py",
        "import oryx\n"
        "class Board(oryx.ConsoleBoard, game='py-board-empty'):\n"
        "    pass\n");

    RunningPython python;
    CHECK_THROWS_AS(python.load(script), ScriptError);
    CHECK_FALSE(ConsoleBoardRegistry::has("py-board-empty"));
}

TEST_CASE("a board class without a game is an intermediate base and registers nothing")
{
    TempDir dir;
    std::filesystem::path script = dir.write("base.py",
        "import oryx\n"
        "class Base(oryx.ConsoleBoard):\n"
        "    pass\n");

    RunningPython python;
    CHECK_NOTHROW(python.load(script));
}

TEST_CASE("register_console_board registers a factory function, and stopping the runtime unregisters the board")
{
    TempDir dir;
    std::filesystem::path script = dir.write("factory.py",
        "import oryx\n"
        "class Board(oryx.ConsoleBoard):\n"
        "    shows_moves = True\n"
        "    def poll_action(self, state):\n"
        "        return 1\n"
        "oryx.register_console_board('py-board-factory', Board, description='from a factory')\n");

    {
        RunningPython python;
        python.load(script);
        CHECK(board_for("py-board-factory")->shows_moves());
    }
    CHECK_FALSE(ConsoleBoardRegistry::has("py-board-factory"));
}

TEST_CASE("a script board for a game that has a C++ board needs overwrite=True, and stopping restores the C++ board")
{
    TempDir dir;
    ConsoleBoardRegistry::register_factory("py-board-cpp", [](const Params&) -> UniquePtr<IConsoleBoard> { return create_unique<ConsoleBoard>(); });

    std::filesystem::path rejected = dir.write("rejected.py",
        "import oryx\n"
        "class Board(oryx.ConsoleBoard, game='py-board-cpp'):\n"
        "    def on_turn(self, state):\n"
        "        pass\n");
    std::filesystem::path replacing = dir.write("replacing.py",
        "import oryx\n"
        "class Board(oryx.ConsoleBoard, game='py-board-cpp', overwrite=True):\n"
        "    shows_moves = True\n"
        "    def on_turn(self, state):\n"
        "        pass\n");

    {
        RunningPython python;
        CHECK_THROWS_AS(python.load(rejected), ScriptError);
        CHECK_FALSE(board_for("py-board-cpp")->shows_moves());

        python.load(replacing);
        CHECK(board_for("py-board-cpp")->shows_moves());
    }
    CHECK_FALSE(board_for("py-board-cpp")->shows_moves());
    ConsoleBoardRegistry::unregister_factory("py-board-cpp");
}

#endif
