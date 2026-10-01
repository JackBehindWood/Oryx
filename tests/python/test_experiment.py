import oryx
import pytest


def tournament(**kwargs):
    return oryx.Tournament("nim", ["random", "first-legal"], matches=40, seed=11, **kwargs)


def test_a_tournament_expands_to_one_matchup_per_seating():
    experiment = tournament()
    keys = [matchup["key"] for matchup in experiment.spec["matchups"]]
    assert keys == ["random vs first-legal", "first-legal vs random"]
    assert isinstance(experiment, oryx.Experiment)


def test_running_gives_per_matchup_counts_that_add_up():
    result = tournament().run()
    assert result.complete
    summary = result.summary()
    assert [row["matches"] for row in summary] == [40, 40]
    for row in summary:
        assert sum(row["wins"]) + row["draws"] == 40
        lower, upper = row["win_rate_intervals"][0]
        assert lower <= row["win_rates"][0] <= upper


def test_the_same_experiment_twice_gives_identical_trials_and_a_different_seed_does_not():
    first = tournament().run()
    assert first.same_trials(tournament().run())
    other = oryx.Tournament("nim", ["random", "random"], matches=200, seed=1).run()
    assert not other.same_trials(oryx.Tournament("nim", ["random", "random"], matches=200, seed=2).run())


def test_save_load_rerun_reproduces_the_counts(tmp_path):
    result = oryx.Tournament("nim", ["random", "first-legal"], matches=30, repeats=2, seed=5, name="demo").run()
    result.save(tmp_path / "out")
    loaded = oryx.ExperimentResult.load(tmp_path / "out")
    assert loaded.same_trials(result)
    assert loaded.spec == result.spec
    assert loaded.metadata["spec_hash"] == result.metadata["spec_hash"]
    assert loaded.rerun().same_trials(loaded)


def test_save_and_load_accept_a_plain_string_path(tmp_path):
    result = tournament().run()
    result.save(str(tmp_path / "text"))
    assert oryx.ExperimentResult.load(str(tmp_path / "text")).same_trials(result)


def test_a_sweep_varies_a_game_parameter_across_matchups():
    experiment = oryx.Experiment.sweep("nim", ["first-legal", "first-legal"], {"game.max_take": [2, 3], "game.stones": [10, 11]}, matches=5)
    params = [(m["game_params"]["max_take"], m["game_params"]["stones"]) for m in experiment.spec["matchups"]]
    assert params == [(2, 10), (2, 11), (3, 10), (3, 11)]
    assert len(experiment.run().summary()) == 4


def test_strategy_parameters_can_be_pinned_and_override_the_derived_seed():
    matchups = [{"game": "nim", "strategies": [("random", {"seed": 3}), "first-legal"]}]
    result = oryx.Experiment(matchups, matches=100, repeats=2, seed=0).run()
    first, second = result.series(result.summary()[0]["matchup"], "wins/0")
    assert first == second


def test_results_convert_to_dicts_and_dataframes():
    result = tournament().run()
    data = result.to_dict()
    assert set(data) == {"spec", "metadata", "complete", "trials", "summary"}
    assert data["metadata"]["oryx_version"] == oryx.__version__
    pandas = pytest.importorskip("pandas")
    frame = result.to_dataframe()
    assert isinstance(frame, pandas.DataFrame)
    assert list(frame.columns) == ["matchup", "repeat", "metric", "value"]
    assert "<table>" in result._repr_html_()


def test_ratings_and_the_cross_table_cover_two_seat_matchups():
    result = oryx.Tournament("nim", ["random", "first-legal"], matches=60, seed=2).run()
    table = result.cross_table()
    assert table["strategies"] == ["random", "first-legal"]
    assert table["scores"][0][1] + table["scores"][1][0] == pytest.approx(1.0)
    assert sum(result.ratings().values()) == pytest.approx(0.0, abs=1e-6)


def test_statistics_helpers_summarise_repeats():
    result = oryx.Tournament("nim", ["random", "first-legal"], matches=30, repeats=4, seed=2).run()
    key = result.summary()[0]["matchup"]
    stats = result.summarize(key, "wins/0")
    assert stats["count"] == 4
    assert stats["mean"] == pytest.approx(sum(result.series(key, "wins/0")) / 4)
    comparison = result.compare(key, key, "wins/0")
    assert comparison["difference"] == 0.0 and not comparison["significant"]


def test_validation_names_the_matchup_and_runs_nothing():
    with pytest.raises(oryx.OryxError, match="unknown strategy 'nope' in seat 1"):
        oryx.Tournament("nim", ["random", "nope"]).validate()
    with pytest.raises(oryx.OryxError, match="depth"):
        oryx.Experiment([{"game": "nim", "strategies": [("random", {"depth": 1}), "random"]}]).run()
    with pytest.raises(oryx.OryxError, match="duplicate"):
        oryx.Experiment([{"game": "nim", "strategies": ["random", "random"]}] * 2).validate()


def test_a_game_must_be_named_by_id_not_passed_as_an_instance():
    with pytest.raises(oryx.OryxError, match="registry id"):
        oryx.Experiment([{"game": 3, "strategies": ["random", "random"]}])


def test_malformed_matchups_and_params_raise_oryx_errors():
    with pytest.raises(oryx.OryxError, match="is a dict"):
        oryx.Experiment([["nim", "random", "random"]])
    with pytest.raises(oryx.OryxError, match="needs both"):
        oryx.Experiment([{"game": "nim"}])
    with pytest.raises(oryx.OryxError, match="must be a dict"):
        oryx.Experiment([{"game": "nim", "strategies": [("random", 3), "random"]}])


def test_unknown_matchups_are_errors_not_empty_series():
    result = tournament().run()
    with pytest.raises(oryx.OryxError, match="no matchup 'typo'"):
        result.series("typo", "wins/0")


def test_progress_is_reported_and_an_error_in_the_callback_stops_the_run():
    seen = []
    tournament().run(lambda done, total: seen.append((done, total)))
    assert seen == [(1, 2), (2, 2)]

    def explode(done, total):
        raise RuntimeError("stop")

    with pytest.raises(RuntimeError, match="stop"):
        oryx.Tournament("nim", ["random", "first-legal"], matches=5, repeats=3).run(explode)


def test_loading_rejects_a_missing_result_directory(tmp_path):
    with pytest.raises(oryx.OryxError, match="has no result.yaml"):
        oryx.ExperimentResult.load(tmp_path)


def test_derived_seeds_reach_script_strategies_as_valid_params():
    import monte_carlo

    experiment = oryx.Experiment.sweep("nim", [monte_carlo.MonteCarlo, "random"], {"seats.0.playouts": [1, 2]}, matches=4, repeats=2, seed=7)
    result = experiment.run()
    assert result.same_trials(experiment.run())
    assert len(result.summary()) == 2
