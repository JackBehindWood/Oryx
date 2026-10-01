import oryx
import pytest

SMALL = {"stones": 7}


def test_a_traced_match_records_one_decision_per_ply():
    match = oryx.Match(oryx.make_game("nim", **SMALL), ["minimax", "random"], trace=True)
    match.play()

    trace = match.trace
    assert len(trace) == len(match.history())
    assert [decision.chosen for decision in trace] == match.history()
    assert [decision.ply for decision in trace] == list(range(len(trace)))
    assert trace[0].player == 0
    assert "minimax/nodes" in trace[0].extra
    assert all(score["value"] is not None for score in trace[0].scores)


def test_an_untraced_match_has_an_empty_trace():
    match = oryx.Match("nim", ["random", "random"])
    match.play()
    assert match.trace == []


def test_a_strategy_that_publishes_nothing_still_appears_in_the_trace():
    match = oryx.Match("nim", ["first-legal", "first-legal"], trace=True)
    match.play()

    decision = match.trace[0]
    assert decision.scores == []
    assert decision.extra == {}
    assert decision.chosen == match.history()[0]


def test_random_publishes_uniform_probabilities():
    match = oryx.Match("nim", ["random", "random"], trace=True)
    match.decide()

    scores = match.trace[0].scores
    assert scores
    assert sum(score["probability"] for score in scores) == pytest.approx(1.0)
    assert len({score["probability"] for score in scores}) == 1


def test_decision_to_dict_is_plain_data():
    match = oryx.Match(oryx.make_game("nim", **SMALL), ["minimax", "random"], trace=True)
    match.decide()

    data = match.trace[0].to_dict()
    assert set(data) == {"ply", "player", "chosen", "chosen_label", "scores", "extra", "tree"}
    assert data["tree"] == []
    assert isinstance(data["chosen_label"], str)


def test_simulate_with_trace_keeps_one_list_per_match():
    result = oryx.simulate(oryx.make_game("nim", **SMALL), ["minimax", "random"], games=5, seed=3, trace=True)

    assert len(result.trace) == 5
    assert sum(len(game) for game in result.trace) == result.decisions
    assert all(game[0].ply == 0 for game in result.trace)


def test_simulate_without_trace_has_no_trace_and_the_same_counts():
    plain = oryx.simulate(oryx.make_game("nim", **SMALL), ["minimax", "random"], games=20, seed=3)
    traced = oryx.simulate(oryx.make_game("nim", **SMALL), ["minimax", "random"], games=20, seed=3, trace=True)

    assert plain.trace is None
    assert plain.to_dict() == traced.to_dict()


def test_a_python_strategy_publishes_through_context_observer():
    class Reporter(oryx.Strategy, id="reporter"):
        def decide(self, context):
            action = context.state.legal_actions()[0]
            if context.observer is not None:
                context.observer.publish(action, values={action: 2.5}, extra={"reporter/calls": 1})
            return action

    match = oryx.Match("nim", ["reporter", "first-legal"], trace=True)
    match.play()

    first = match.trace[0]
    assert first.scores[0]["value"] == 2.5
    assert first.extra == {"reporter/calls": 1.0}
    assert sum(1 for decision in match.trace if decision.player == 0) == len([d for d in match.trace if "reporter/calls" in d.extra])


def test_the_observer_is_none_when_nothing_is_observing_and_dead_after_decide():
    seen = []

    class Peek(oryx.Strategy, id="observer-peek"):
        def decide(self, context):
            seen.append(context.observer)
            return context.state.legal_actions()[0]

    oryx.Match("nim", ["observer-peek", "first-legal"]).decide()
    assert seen == [None]

    traced = oryx.Match("nim", ["observer-peek", "first-legal"], trace=True)
    traced.decide()
    assert seen[1] is not None
    with pytest.raises(oryx.OryxError):
        seen[1].publish(0)


def test_publishing_an_illegal_action_is_rejected():
    class Cheat(oryx.Strategy, id="observer-cheat"):
        def decide(self, context):
            context.observer.publish(10**6)
            return context.state.legal_actions()[0]

    match = oryx.Match("nim", ["observer-cheat", "first-legal"], trace=True)
    with pytest.raises(oryx.OryxError):
        match.decide()


def test_publishing_scores_for_an_illegal_action_is_rejected():
    class Cheat(oryx.Strategy, id="observer-cheat-scores"):
        def decide(self, context):
            context.observer.publish(context.state.legal_actions()[0], probabilities={10**6: 1.0})
            return context.state.legal_actions()[0]

    match = oryx.Match("nim", ["observer-cheat-scores", "first-legal"], trace=True)
    with pytest.raises(oryx.OryxError):
        match.decide()


def test_attached_and_detached_runs_play_identical_games():
    for strategies in (["minimax", "random"], ["random", "minimax"], ["random", "random"], ["first-legal", "minimax"]):
        plain = oryx.simulate(oryx.make_game("nim", **SMALL), strategies, games=15, seed=11)
        traced = oryx.simulate(oryx.make_game("nim", **SMALL), strategies, games=15, seed=11, trace=True)
        assert plain.to_dict() == traced.to_dict()


def test_experiment_diagnostics_are_opt_in_and_reproducible():
    experiment = oryx.Experiment(
        [{"game": "nim", "game_params": SMALL, "strategies": ["minimax", "random"]}], matches=4, repeats=2, seed=9
    )

    plain = experiment.run()
    observed = experiment.run(diagnostics=True)

    assert observed.series(observed.summary()[0]["matchup"], "minimax/nodes")
    assert not any(plain.series(plain.summary()[0]["matchup"], "minimax/nodes"))
    assert observed.rerun(diagnostics=True).same_trials(observed)
