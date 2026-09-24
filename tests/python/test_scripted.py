import typing

import monte_carlo
import nim
import oryx
import pytest


def test_a_class_deriving_from_oryx_game_registers_with_a_schema_built_from_its_typed_fields():
    info = oryx.describe_game("nim")
    assert info["description"] == "Players alternate taking 1..max_take stones from a pile; whoever takes the last stone wins."
    assert info["params"] == [
        {"name": "stones", "type": "int", "description": "", "required": False, "default": 21},
        {"name": "max_take", "type": "int", "description": "", "required": False, "default": 3},
    ]
    origin = info["origin"]
    assert origin["language"] == "python"
    assert origin["module"] == "nim"
    assert origin["source_file"].endswith("nim.py")


def test_make_game_builds_a_python_game_from_keyword_parameters_and_validates_them():
    game = oryx.make_game("nim", stones=15, max_take=4)
    state = game.new_initial_state()
    assert game.name() == "Nim"
    assert game.num_players() == 2
    assert state.legal_actions() == [1, 2, 3, 4]

    state.apply(4)
    assert state.legal_actions() == [1, 2, 3, 4]
    assert state.current_player() == 1
    assert state.action_to_string(2) == "take 2 (leaves 9)"

    assert oryx.make_game("nim").new_initial_state().legal_actions() == [1, 2, 3]

    with pytest.raises(oryx.ParamError) as excinfo:
        oryx.make_game("nim", stones="x")
    assert excinfo.value.key == "stones"
    with pytest.raises(oryx.ParamError) as excinfo:
        oryx.make_game("nim", pile=3)
    assert excinfo.value.key == "pile"


def test_a_python_game_plays_against_cpp_strategies_through_match_and_simulate():
    match = oryx.Match("nim", ["first-legal", "first-legal"])
    assert match.play() == [1.0, -1.0]
    assert len(match.history()) == 21

    result = oryx.simulate("nim", ["random", "random"], games=40, seed=3)
    assert result.matches == 40
    assert sum(result.wins) == 40
    assert result.draws == 0

    result = oryx.simulate(nim.Nim(), ["first-legal", "first-legal"], games=2)
    assert result.wins == [2, 0]


def test_a_strategys_parameters_are_set_before_init_runs_and_simulate_seeds_strategies_that_declare_seed():
    a = oryx.simulate("nim", ["monte-carlo", "random"], games=6, seed=4)
    b = oryx.simulate("nim", ["monte-carlo", "random"], games=6, seed=4)
    assert a.wins == b.wins
    assert a.decisions == b.decisions
    assert oryx.describe_strategy("monte-carlo")["params"] == [
        {"name": "playouts", "type": "int", "description": "", "required": False, "default": 30},
        {"name": "seed", "type": "int", "description": "", "required": False, "default": 1},
    ]


def test_bad_class_definitions_fail_at_import_with_a_message_naming_the_problem():
    def attempt(source):
        try:
            exec(source, {"oryx": oryx, "typing": typing})
        except oryx.ScriptError as e:
            return str(e).split(":")[0]
        return None

    assert attempt('class A(oryx.Game, id="a"): pass') == "A must define new_initial_state() to be registered as the game 'a'"
    assert attempt('class B(oryx.Strategy, id="b"): pass') == "B must define decide() to be registered as the strategy 'b'"
    assert (
        attempt('class C(oryx.Game, id="c", bogus=1):\n    num_players = 2\n    def new_initial_state(self): pass')
        == "class C"
    )
    assert attempt(
        'class D(oryx.Game, id="d"):\n    history: list = []\n    def new_initial_state(self): pass'
    ) == "D.history is annotated with an unsupported type (fields are parameters of type bool, int, float or str; use typing.ClassVar for anything else)"
    assert attempt(
        'class E(oryx.Game, id="e"):\n    num_players: int = 2\n    def new_initial_state(self): pass'
    ) == "E.num_players is reserved and cannot be a parameter; assign it without an annotation"
    assert (
        attempt('class F(oryx.Game, id="f"):\n    stones: int = "many"\n    def new_initial_state(self): pass')
        == "F.stones is annotated as int but its default has another type"
    )

    class Fine(oryx.Game, id="fine"):
        label: typing.ClassVar[str] = "x"
        num_players = 2

        def new_initial_state(self):
            pass

    assert oryx.describe_game("fine")["params"] == []


def test_a_python_game_without_num_players_is_rejected_when_it_is_created():
    class NoPlayers(oryx.Game, id="no-players"):
        def new_initial_state(self):
            pass

    with pytest.raises(oryx.ScriptError, match="NoPlayers must define num_players"):
        oryx.make_game("no-players")


def test_an_exception_in_a_python_method_surfaces_as_scripterror_with_its_traceback_and_does_not_end_the_process():
    class BoomState:
        def legal_actions(self):
            return [0]

        def apply(self, action):
            raise RuntimeError("kaboom")

        def undo(self, action):
            pass

        def current_player(self):
            return 0

        def is_terminal(self):
            return False

        def outcome(self):
            return [0.0, 0.0]

        def action_to_string(self, action):
            return "x"

    class Boom(oryx.Game, id="boom"):
        num_players = 2

        def new_initial_state(self):
            return BoomState()

    with pytest.raises(oryx.ScriptError) as excinfo:
        oryx.simulate("boom", ["first-legal", "first-legal"], games=1)
    assert str(excinfo.value).split("\n")[0] == "state.apply(): RuntimeError: kaboom"
    assert "RuntimeError: kaboom" in excinfo.value.detail


def test_a_state_class_missing_a_required_method_is_reported_when_the_state_is_created_naming_the_method():
    class Broken(oryx.Game, id="broken"):
        num_players = 2

        def new_initial_state(self):
            return BrokenState()

    class BrokenState:
        def apply(self, action):
            pass

    with pytest.raises(oryx.ScriptError, match="state class 'BrokenState' must define legal_actions()"):
        oryx.make_game("broken").new_initial_state()


def test_a_script_method_returning_the_wrong_type_is_a_scripterror_naming_the_method_and_the_type():
    class Wrong(oryx.Game, id="wrong"):
        num_players = 2

        def new_initial_state(self):
            return WrongState()

    class WrongState(oryx.State):
        def legal_actions(self):
            return "nope"

        def apply(self, action):
            pass

        def undo(self, action):
            pass

        def current_player(self):
            return "zero"

        def is_terminal(self):
            return []

        def outcome(self):
            return [0.0, 0.0]

    state = oryx.make_game("wrong").new_initial_state()
    with pytest.raises(oryx.ScriptError, match="state.legal_actions\\(\\): expected a sequence of ints, got str"):
        state.legal_actions()
    with pytest.raises(oryx.ScriptError, match="state.current_player\\(\\): expected an int, got str"):
        state.current_player()
    with pytest.raises(oryx.ScriptError, match="state.is_terminal\\(\\): expected a bool, got list"):
        state.is_terminal()


def test_a_game_without_action_features_gives_the_strategy_none():
    seen = []

    class Peek(oryx.Strategy, id="peek"):
        def decide(self, context):
            seen.append(context.action_features)
            return context.state.legal_actions()[0]

    oryx.Match("nim", ["peek", "first-legal"]).decide()
    assert seen == [None]


def test_a_game_whose_new_initial_state_returns_something_that_is_not_a_state_fails_with_a_scripterror_naming_it():
    class NoState(oryx.Game, id="no-state"):
        num_players = 2

        def new_initial_state(self):
            return None

    with pytest.raises(oryx.ScriptError, match="game.new_initial_state\\(\\): expected a state, got NoneType"):
        oryx.make_game("no-state").new_initial_state()


def test_register_game_rejects_a_factory_that_is_not_callable_and_a_class_that_gives_an_empty_id():
    with pytest.raises(oryx.ScriptError, match="the factory for the game 'bad-factory' must be callable, got 'int'"):
        oryx.register_game("bad-factory", 42)
    with pytest.raises(oryx.ScriptError, match="class E: id cannot be empty"):
        exec(
            "class E(oryx.Game, id=''):\n    num_players = 2\n    def new_initial_state(self): pass",
            {"oryx": oryx},
        )


def test_parameters_accept_any_object_that_behaves_as_an_int_or_a_float_such_as_a_numpy_scalar():
    class Index:
        def __index__(self):
            return 7

    class Real:
        def __float__(self):
            return 2.5

    class Scaled(oryx.Game, id="scaled"):
        scale: float = 1.0
        stones: int = 3
        num_players = 2

        def new_initial_state(self):
            return None

    game = oryx.make_game("nim", stones=Index())
    assert len(game.new_initial_state().legal_actions()) == 3
    oryx.make_game("scaled", scale=Real(), stones=Index())


def test_string_annotations_as_produced_by_future_import_annotations_still_declare_parameters():
    code = (
        "from __future__ import annotations\n"
        "import oryx\n"
        "class Lazy(oryx.Game, id='lazy'):\n"
        "    stones: int = 4\n"
        "    label: str\n"
        "    num_players = 2\n"
        "    def new_initial_state(self): return None\n"
    )
    exec(code, {})
    assert [(p["name"], p["type"], p["required"]) for p in oryx.describe_game("lazy")["params"]] == [
        ("stones", "int", False),
        ("label", "string", True),
    ]


def test_legal_actions_is_asked_of_the_script_once_per_position():
    calls = [0]

    class Counting(oryx.Game, id="counting"):
        num_players = 2

        def new_initial_state(self):
            return CountingState()

    class CountingState(oryx.State):
        def __init__(self):
            self.stones = 5

        def legal_actions(self):
            calls[0] += 1
            return [1, 2] if self.stones > 1 else [1]

        def apply(self, action):
            self.stones -= action

        def undo(self, action):
            self.stones += action

        def current_player(self):
            return 0

        def is_terminal(self):
            return self.stones == 0

        def outcome(self):
            return [0.0, 0.0]

    state = oryx.make_game("counting").new_initial_state()
    state.legal_actions()
    state.legal_actions()
    assert calls[0] == 1

    state.apply(2)
    assert calls[0] == 1
    assert state.legal_actions() == [1, 2]
    assert calls[0] == 2

    state.undo(2)
    assert state.legal_actions() == [1, 2]
    assert calls[0] == 3


def test_oryx_state_supplies_a_default_action_to_string():
    class Counter(oryx.State):
        pass

    assert Counter().action_to_string(3) == "3"
    assert isinstance(Counter(), oryx.State)


def test_register_game_and_register_strategy_register_factory_functions_with_parameters():
    class Small(nim.Nim):
        pass

    def make_small(stones, label):
        game = Small()
        game.stones = stones
        return game

    oryx.register_game("nim-small", make_small, params={"stones": 7, "label": str}, description="Small nim")
    info = oryx.describe_game("nim-small")
    assert info["description"] == "Small nim"
    assert info["params"] == [
        {"name": "stones", "type": "int", "description": "", "required": False, "default": 7},
        {"name": "label", "type": "string", "description": "", "required": True},
    ]
    assert info["origin"]["module"] == make_small.__module__

    game = oryx.make_game("nim-small", label="x", stones=5)
    assert game.new_initial_state().legal_actions() == [1, 2, 3]

    with pytest.raises(oryx.ParamError, match="'nim-small': parameter 'label' is required"):
        oryx.make_game("nim-small")


def test_the_base_classes_placeholder_methods_raise_notimplementederror_and_never_count_as_definitions():
    class Lazy(oryx.Strategy):
        pass

    with pytest.raises(NotImplementedError, match="Lazy must define decide()"):
        Lazy().decide(None)

    class HalfState(oryx.State):
        def apply(self, action):
            pass

    class Half(oryx.Game, id="half"):
        num_players = 2

        def new_initial_state(self):
            return HalfState()

    with pytest.raises(oryx.ScriptError, match="state class 'HalfState' must define legal_actions()"):
        oryx.make_game("half").new_initial_state()


def test_match_simulate_and_benchmark_accept_registered_classes_and_one_strategy_fills_every_seat():
    Nim = nim.Nim
    MonteCarlo = monte_carlo.MonteCarlo

    match = oryx.Match(Nim, [MonteCarlo, "random"])
    assert match.state().current_player() == 0

    result = oryx.simulate(Nim, MonteCarlo, games=4, seed=3)
    assert result.metadata["strategies"] == ["monte-carlo", "monte-carlo"]

    assert oryx.simulate("nim", ("random", "first-legal"), games=2).metadata["strategies"] == ["random", "first-legal"]
    assert oryx.benchmark.benchmark(Nim, "random", games=2).outcome.matches == 2
    assert oryx.describe_strategy(MonteCarlo)["name"] == "monte-carlo"
    assert oryx.simulate(Nim, [oryx.make_strategy(MonteCarlo, playouts=2), "random"], games=2).matches == 2

    class Unregistered(oryx.Strategy):
        def decide(self, context):
            return 1

    with pytest.raises(
        oryx.OryxError,
        match='the class Unregistered is not registered; give it an id \\(`class Unregistered\\(..., id="..."\\)`\\) to pass the class itself',
    ):
        oryx.Match(Nim, Unregistered)


def test_a_class_resolves_through_its_registry_entry_so_seeding_by_name_applies_to_it():
    Nim = nim.Nim
    MonteCarlo = monte_carlo.MonteCarlo
    by_class = oryx.simulate(Nim, [MonteCarlo, "random"], games=6, seed=11)
    by_name = oryx.simulate("nim", ["monte-carlo", "random"], games=6, seed=11)
    assert by_class.wins == by_name.wins
    assert by_class.decisions == by_name.decisions
