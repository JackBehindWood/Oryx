#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

// numpy cannot be imported again after an interpreter restart, so every check that really imports it lives in this one case and no other test may.
TEST_CASE("numpy and pandas interoperate with results and math when installed")
{
    std::string output = run_oryx_script(
        "try:\n"
        "    import numpy, pandas\n"
        "except ImportError:\n"
        "    mark('skipped')\n"
        "else:\n"
        "    r = oryx.simulate('tictactoe', ['first-legal', 'random'], games=20, seed=3)\n"
        "    arrays = r.to_numpy()\n"
        "    assert sorted(arrays) == ['mean_rewards', 'rewards', 'win_rates', 'wins']\n"
        "    assert isinstance(arrays['wins'], numpy.ndarray) and arrays['wins'].tolist() == r.wins\n"
        "    assert arrays['win_rates'].tolist() == r.win_rates\n"
        "    frame = r.to_dataframe()\n"
        "    assert list(frame.columns) == ['player', 'wins', 'win_rate', 'mean_reward'] and len(frame) == 2\n"
        "    assert frame['wins'].tolist() == r.wins\n"
        "    assert frame.attrs['matches'] == 20 and frame.attrs['metadata']['game'] == 'TicTacToe'\n"
        "    v = oryx.math.Vec3(1, 2, 3)\n"
        "    assert numpy.asarray(v).tolist() == [1.0, 2.0, 3.0] and numpy.asarray(v).dtype == numpy.float64\n"
        "    assert numpy.asarray(oryx.math.Mat2([[1, 2], [3, 4]])).tolist() == [[1.0, 2.0], [3.0, 4.0]]\n"
        "    assert oryx.math.Vec3(numpy.array([4.0, 5.0, 6.0])) == oryx.math.Vec3(4, 5, 6)\n"
        "    assert oryx.math.Vec2(numpy.float64(2.0)) == oryx.math.Vec2(2, 2)\n"
        "    mark('ok')\n");

    if (output == "skipped")
    {
        MESSAGE("numpy or pandas is not installed (uv sync installs the research group); nothing was checked");
        return;
    }
    CHECK(output == "ok");
}

#endif
