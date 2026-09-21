#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("a BatchResult derives its rates from the counts")
{
    std::string output = run_oryx_script(
        "r = oryx.simulate('tictactoe', ['first-legal', 'random'], games=40, seed=3)\n"
        "mark(str(r.matches) + '|' + str(sum(r.wins) + r.draws) + '|')\n"
        "mark(str(r.win_rates == [w / 40 for w in r.wins]) + str(r.draw_rate == r.draws / 40) + str(r.mean_rewards == [x / 40 for x in r.rewards]))\n"
        "empty = oryx.BatchRunner('tictactoe', ['first-legal', 'first-legal']).run(0)\n"
        "mark('|' + str(empty.win_rates) + str(empty.draw_rate) + str(empty.mean_rewards) + str(empty.wins))\n");

    CHECK(output == "40|40|TrueTrueTrue|[0.0, 0.0]0.0[0.0, 0.0][0, 0]");
}

TEST_CASE("simulate records what it ran and a BatchRunner result has no metadata")
{
    std::string output = run_oryx_script(
        "class Mine(oryx.Strategy):\n"
        "    def decide(self, context):\n"
        "        return context.state.legal_actions()[0]\n"
        "r = oryx.simulate('tictactoe', ['random', Mine()], games=5, seed=9)\n"
        "m = r.metadata\n"
        "mark(str(m['game']) + str(m['strategies']) + str(m['games']) + str(m['seed']) + m['oryx_version'] + '|')\n"
        "mark(str(oryx.simulate('tictactoe', ['first-legal', 'first-legal'], games=1).metadata['seed']) + '|')\n"
        "mark(str(oryx.BatchRunner('tictactoe', ['first-legal', 'first-legal']).run(1).metadata))\n");

    CHECK(output == "TicTacToe['random', 'Mine']590.1.0|None|None");
}

TEST_CASE("to_dict carries the counts, the rates and the metadata")
{
    std::string output = run_oryx_script(
        "r = oryx.simulate('tictactoe', ['first-legal', 'first-legal'], games=4, seed=1)\n"
        "d = r.to_dict()\n"
        "mark(str(sorted(d)) + '|' + str(d['wins']) + str(d['rewards']) + str(d['win_rates']) + str(d['metadata']['games']))\n");

    CHECK(output ==
          "['decisions', 'draw_rate', 'draws', 'matches', 'mean_rewards', 'metadata', 'rewards', 'win_rates', 'wins']|"
          "[4, 0][4.0, -4.0][1.0, 0.0]4");
}

TEST_CASE("the HTML repr is a table that escapes strategy names")
{
    std::string output = run_oryx_script(
        "class Odd(oryx.Strategy, id='<b>&'):\n"
        "    def decide(self, context):\n"
        "        return context.state.legal_actions()[0]\n"
        "html = oryx.simulate('tictactoe', ['<b>&', 'first-legal'], games=2)._repr_html_()\n"
        "mark(str(html.startswith('<table>')) + str('<b>' in html) + str('&lt;b&gt;&amp;' in html) + str(html.count('<tr>')) + str('vs first-legal' in html))\n");

    CHECK(output == "TrueFalseTrue3True");
}

TEST_CASE("to_numpy and to_dataframe explain what to install when the package is missing")
{
    std::string output = run_oryx_script(
        "import sys\n"
        "sys.modules['numpy'] = None\n"
        "sys.modules['pandas'] = None\n"
        "r = oryx.BatchRunner('tictactoe', ['first-legal', 'first-legal']).run(1)\n"
        "for call in (r.to_numpy, r.to_dataframe):\n"
        "    try:\n"
        "        call()\n"
        "    except oryx.OryxError as e:\n"
        "        mark(str(e) + ';')\n");

    CHECK(output ==
          "BatchResult.to_numpy() needs numpy; install it with `pip install numpy`;"
          "BatchResult.to_dataframe() needs pandas; install it with `pip install pandas`;");
}

#endif
