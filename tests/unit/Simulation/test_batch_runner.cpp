#include "doctest.h"

#include "../Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

Outcome make_outcome(double reward_a, double reward_b)
{
    Outcome outcome;
    outcome.is_terminal = true;
    outcome.rewards = Rewards<double>(2);
    outcome.rewards[0] = reward_a;
    outcome.rewards[1] = reward_b;
    return outcome;
}

BatchResult make_result()
{
    BatchResult result;
    result.wins.assign(2, 0);
    result.rewards = Rewards<double>(2);
    return result;
}

} // namespace

TEST_CASE("accumulate() records a win for the player with the strictly-highest reward")
{
    BatchResult result = make_result();

    accumulate(result, make_outcome(1.0, -1.0));

    CHECK(result.matches == 1);
    CHECK(result.wins == SmallVector<int32_t, 2>{ 1, 0 });
    CHECK(result.draws == 0);
    CHECK(result.rewards[0] == doctest::Approx(1.0));
    CHECK(result.rewards[1] == doctest::Approx(-1.0));
}

TEST_CASE("accumulate() records a win for the other player symmetrically")
{
    BatchResult result = make_result();

    accumulate(result, make_outcome(-1.0, 1.0));

    CHECK(result.wins == SmallVector<int32_t, 2>{ 0, 1 });
}

TEST_CASE("accumulate() records a draw when no player strictly leads")
{
    BatchResult result = make_result();

    accumulate(result, make_outcome(0.0, 0.0));

    CHECK(result.draws == 1);
    CHECK(result.wins == SmallVector<int32_t, 2>{ 0, 0 });
}

TEST_CASE("accumulate() sums rewards and matches across repeated calls")
{
    BatchResult result = make_result();

    accumulate(result, make_outcome(1.0, -1.0));
    accumulate(result, make_outcome(1.0, -1.0));

    CHECK(result.matches == 2);
    CHECK(result.rewards[0] == doctest::Approx(2.0));
    CHECK(result.rewards[1] == doctest::Approx(-2.0));
}

// Misère take-1..3 Nim from a pile of 2: the only optimal move is take=1,
// leaving pile=1 (a P-position) - whatever the second player does from
// there, they must take the last stone and lose. So MinimaxStrategy as
// player 0 always beats FirstLegalStrategy as player 1, deterministically
// (docs/design/quality.md - avoid a flaky stochastic assertion).
TEST_CASE("BatchRunner::run aggregates a deterministic pairing correctly")
{
    DummyGame game(2);
    MinimaxStrategy minimax;
    FirstLegalStrategy first_legal;

    BatchRunner runner(game, { &minimax, &first_legal });
    BatchResult result = runner.run(5);

    CHECK(result.matches == 5);
    CHECK(result.wins == SmallVector<int32_t, 2>{ 5, 0 });
    CHECK(result.draws == 0);
    CHECK(result.rewards[0] == doctest::Approx(5.0));
    CHECK(result.rewards[1] == doctest::Approx(-5.0));
}

TEST_CASE("win_rate, draw_rate and mean_reward divide by the matches played")
{
    BatchResult result = make_result();
    accumulate(result, make_outcome(1.0, -1.0));
    accumulate(result, make_outcome(1.0, -1.0));
    accumulate(result, make_outcome(0.0, 0.0));
    accumulate(result, make_outcome(-1.0, 1.0));

    CHECK(win_rate(result, 0) == doctest::Approx(0.5));
    CHECK(win_rate(result, 1) == doctest::Approx(0.25));
    CHECK(draw_rate(result) == doctest::Approx(0.25));
    CHECK(mean_reward(result, 0) == doctest::Approx(0.25));
    CHECK(mean_reward(result, 1) == doctest::Approx(-0.25));
}

TEST_CASE("the rates of an empty batch are zero")
{
    BatchResult result = make_result();

    CHECK(win_rate(result, 0) == 0.0);
    CHECK(draw_rate(result) == 0.0);
    CHECK(mean_reward(result, 1) == 0.0);
}
