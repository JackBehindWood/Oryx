import oryx
import pytest


def test_match_steps_a_game_by_hand_with_undo_redo_and_history():
    match = oryx.Match("nim", ["first-legal", "first-legal"])
    state = match.state()
    assert state.legal_actions() == [1, 2, 3]

    match.apply(1)
    assert match.history() == [1]
    assert state.current_player() == 1
    assert match.current_player() == 1

    assert match.decide() == 1
    assert match.undo() == 1
    assert match.undo() is None
    assert match.redo() == 1
    assert match.history() == [1]

    assert state.action_to_string(1) == "take 1 (leaves 19)"


def test_the_state_handle_of_a_match_is_read_only_so_the_match_history_cannot_be_bypassed():
    match = oryx.Match("nim", ["first-legal", "first-legal"])
    state = match.state()
    match.apply(1)

    for call in (lambda: state.apply(1), lambda: state.undo(1)):
        with pytest.raises(oryx.OryxError, match="this state belongs to a match and is read-only: use match.apply\\(\\) and match.undo\\(\\)"):
            call()

    assert match.undo() == 1
    assert match.history() == []
    assert state.legal_actions() == [1, 2, 3]


def test_match_rejects_illegal_actions_and_mismatched_strategy_counts_without_touching_the_state():
    match = oryx.Match("nim", ["first-legal", "first-legal"])
    match.apply(1)

    with pytest.raises(oryx.IllegalActionError, match="action 99 is not legal in this state"):
        match.apply(99)
    with pytest.raises(oryx.OryxError, match="Nim has 2 players but 1 strategies were given"):
        oryx.Match("nim", ["random"])

    assert match.history() == [1]


def test_match_plays_to_the_end_and_reports_the_rewards():
    match = oryx.Match("nim", ["first-legal", "first-legal"])
    assert match.play() == [1.0, -1.0]
    assert match.is_terminal() is True
    assert match.outcome() == [1.0, -1.0]
    assert len(match.history()) == 21


def test_simulate_leaves_strategy_instances_unseeded_by_the_master_seed():
    mine = oryx.make_strategy("random", seed=5)
    r = oryx.simulate("nim", [mine, "first-legal"], games=10, seed=1)
    assert r.matches == 10
    assert repr(r).startswith("<oryx.BatchResult matches=10 wins=[")


def test_batch_runner_runs_repeatedly_and_rejects_a_negative_count():
    runner = oryx.BatchRunner("nim", ["first-legal", "first-legal"])
    r = runner.run(3)
    assert (r.matches, r.wins, r.draws, r.decisions, r.rewards) == (3, [3, 0], 0, 63, [3.0, -3.0])

    with pytest.raises(oryx.OryxError, match="the number of matches cannot be negative"):
        runner.run(-1)
