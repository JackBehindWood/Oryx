import oryx
import pytest


def test_numpy_and_pandas_interoperate_with_results_and_math_when_installed():
    numpy = pytest.importorskip("numpy")
    pytest.importorskip("pandas")

    r = oryx.simulate("nim", ["first-legal", "random"], games=20, seed=3)
    arrays = r.to_numpy()
    assert sorted(arrays) == ["mean_rewards", "rewards", "win_rates", "wins"]
    assert isinstance(arrays["wins"], numpy.ndarray)
    assert arrays["wins"].tolist() == r.wins
    assert arrays["win_rates"].tolist() == r.win_rates

    frame = r.to_dataframe()
    assert list(frame.columns) == ["player", "wins", "win_rate", "mean_reward"]
    assert len(frame) == 2
    assert frame["wins"].tolist() == r.wins
    assert frame.attrs["matches"] == 20
    assert frame.attrs["metadata"]["game"] == "Nim"

    v = oryx.math.Vec3(1, 2, 3)
    assert numpy.asarray(v).tolist() == [1.0, 2.0, 3.0]
    assert numpy.asarray(v).dtype == numpy.float64
    assert numpy.asarray(oryx.math.Mat2([[1, 2], [3, 4]])).tolist() == [[1.0, 2.0], [3.0, 4.0]]
    assert oryx.math.Vec3(numpy.array([4.0, 5.0, 6.0])) == oryx.math.Vec3(4, 5, 6)
    assert oryx.math.Vec2(numpy.float64(2.0)) == oryx.math.Vec2(2, 2)
