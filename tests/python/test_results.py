import sys

import oryx
import pytest


def test_batch_result_derives_rates_from_the_counts():
    r = oryx.simulate("nim", ["first-legal", "random"], games=40, seed=3)
    assert r.matches == 40
    assert sum(r.wins) + r.draws == 40
    assert r.win_rates == [w / 40 for w in r.wins]
    assert r.draw_rate == r.draws / 40
    assert r.mean_rewards == [x / 40 for x in r.rewards]

    empty = oryx.BatchRunner("nim", ["first-legal", "first-legal"]).run(0)
    assert empty.win_rates == [0.0, 0.0]
    assert empty.draw_rate == 0.0
    assert empty.mean_rewards == [0.0, 0.0]
    assert empty.wins == [0, 0]


def test_simulate_records_what_it_ran_and_a_batch_runner_result_has_no_metadata():
    class Mine(oryx.Strategy):
        def decide(self, context):
            return context.state.legal_actions()[0]

    r = oryx.simulate("nim", ["random", Mine()], games=5, seed=9)
    m = r.metadata
    assert m["game"] == "Nim"
    assert m["strategies"] == ["random", "Mine"]
    assert m["games"] == 5
    assert m["seed"] == 9
    assert m["oryx_version"] == "0.1.0"

    assert oryx.simulate("nim", ["first-legal", "first-legal"], games=1).metadata["seed"] is None
    assert oryx.BatchRunner("nim", ["first-legal", "first-legal"]).run(1).metadata is None


def test_to_dict_carries_the_counts_the_rates_and_the_metadata():
    r = oryx.simulate("nim", ["first-legal", "first-legal"], games=4, seed=1)
    d = r.to_dict()
    assert sorted(d) == [
        "decisions",
        "draw_rate",
        "draws",
        "matches",
        "mean_rewards",
        "metadata",
        "rewards",
        "win_rates",
        "wins",
    ]
    assert d["wins"] == [4, 0]
    assert d["rewards"] == [4.0, -4.0]
    assert d["win_rates"] == [1.0, 0.0]
    assert d["metadata"]["games"] == 4


def test_the_html_repr_is_a_table_that_escapes_strategy_names():
    class Odd(oryx.Strategy, id="<b>&"):
        def decide(self, context):
            return context.state.legal_actions()[0]

    html = oryx.simulate("nim", ["<b>&", "first-legal"], games=2)._repr_html_()
    assert html.startswith("<table>")
    assert "<b>" not in html
    assert "&lt;b&gt;&amp;" in html
    assert html.count("<tr>") == 3
    assert "vs first-legal" in html


def test_to_numpy_and_to_dataframe_explain_what_to_install_when_the_package_is_missing(monkeypatch):
    monkeypatch.setitem(sys.modules, "numpy", None)
    monkeypatch.setitem(sys.modules, "pandas", None)
    r = oryx.BatchRunner("nim", ["first-legal", "first-legal"]).run(1)
    with pytest.raises(oryx.OryxError, match=r"BatchResult\.to_numpy\(\) needs numpy; install it with `pip install numpy`"):
        r.to_numpy()
    with pytest.raises(oryx.OryxError, match=r"BatchResult\.to_dataframe\(\) needs pandas; install it with `pip install pandas`"):
        r.to_dataframe()
