#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

StrategySpec strategy(const std::string& id, Params params = {})
{
    return StrategySpec{ id, std::move(params) };
}

Matchup tictactoe_matchup(const std::string& first, const std::string& second)
{
    Matchup matchup;
    matchup.game = "tictactoe";
    matchup.seats = { strategy(first), strategy(second) };
    return matchup;
}

ExperimentSpec make_spec(std::vector<Matchup> matchups)
{
    ExperimentSpec spec;
    spec.name = "test";
    spec.matchups = std::move(matchups);
    return spec;
}

} // namespace

TEST_CASE("matchup_key() prefers the label and otherwise describes the content, not the position")
{
    Matchup matchup = tictactoe_matchup("random", "first-legal");
    CHECK(matchup_key(matchup) == "tictactoe|random|first-legal");

    matchup.seats[0].params["seed"] = int64_t{ 3 };
    CHECK(matchup_key(matchup) == "tictactoe|random(seed=i:3)|first-legal");

    matchup.label = "mine";
    CHECK(matchup_key(matchup) == "mine");
}

TEST_CASE("spec_hash() is stable across calls and changes with the content")
{
    ExperimentSpec spec = make_spec({ tictactoe_matchup("random", "first-legal") });
    CHECK(spec_hash(spec) == spec_hash(spec));

    ExperimentSpec other = spec;
    other.master_seed = 1;
    CHECK(spec_hash(other) != spec_hash(spec));
}

TEST_CASE("sweep() with no axes returns the base matchup")
{
    std::vector<Matchup> matchups = sweep(tictactoe_matchup("random", "first-legal"), {});

    REQUIRE(matchups.size() == 1);
    CHECK(matchups[0].label.empty());
}

TEST_CASE("sweep() takes the cartesian product with the last axis varying fastest")
{
    std::vector<SweepAxis> axes = {
        { "seats.0.seed", { ParamValue{ int64_t{ 1 } }, ParamValue{ int64_t{ 2 } } } },
        { "seats.1.seed", { ParamValue{ int64_t{ 10 } }, ParamValue{ int64_t{ 20 } }, ParamValue{ int64_t{ 30 } } } },
    };

    std::vector<Matchup> matchups = sweep(tictactoe_matchup("random", "random"), axes);

    REQUIRE(matchups.size() == 6);
    CHECK(std::get<int64_t>(matchups[0].seats[0].params.at("seed")) == 1);
    CHECK(std::get<int64_t>(matchups[0].seats[1].params.at("seed")) == 10);
    CHECK(std::get<int64_t>(matchups[1].seats[1].params.at("seed")) == 20);
    CHECK(std::get<int64_t>(matchups[3].seats[0].params.at("seed")) == 2);
    CHECK(std::get<int64_t>(matchups[5].seats[1].params.at("seed")) == 30);

    std::set<std::string> keys;
    for (const Matchup& matchup : matchups)
    {
        keys.insert(matchup_key(matchup));
    }
    CHECK(keys.size() == 6);
}

TEST_CASE("sweep() rejects malformed paths and empty axes")
{
    Matchup base = tictactoe_matchup("random", "random");

    CHECK_THROWS_AS(sweep(base, { { "seed", { ParamValue{ int64_t{ 1 } } } } }), ExperimentError);
    CHECK_THROWS_AS(sweep(base, { { "seats.2.seed", { ParamValue{ int64_t{ 1 } } } } }), ExperimentError);
    CHECK_THROWS_AS(sweep(base, { { "seats.x.seed", { ParamValue{ int64_t{ 1 } } } } }), ExperimentError);
    CHECK_THROWS_AS(sweep(base, { { "seats.99999999999999999999.seed", { ParamValue{ int64_t{ 1 } } } } }), ExperimentError);
    CHECK_THROWS_AS(sweep(base, { { "game.size", {} } }), ExperimentError);
}

TEST_CASE("round_robin() pairs the pool, and rotating seats doubles it")
{
    std::vector<StrategySpec> pool = { strategy("random"), strategy("first-legal"), strategy("minimax") };

    CHECK(round_robin("tictactoe", {}, pool, false).size() == 3);

    std::vector<Matchup> rotated = round_robin("tictactoe", {}, pool, true);
    REQUIRE(rotated.size() == 6);
    CHECK(rotated[0].seats[0].id == "random");
    CHECK(rotated[0].seats[1].id == "first-legal");
    CHECK(rotated[1].seats[0].id == "first-legal");
    CHECK(rotated[1].seats[1].id == "random");
    CHECK(rotated[0].label == "random vs first-legal");
}

TEST_CASE("round_robin() does not rotate a pair of identical strategies into a duplicate matchup")
{
    std::vector<Matchup> matchups = round_robin("tictactoe", {}, { strategy("random"), strategy("random") }, true);

    CHECK(matchups.size() == 1);
    CHECK_NOTHROW(validate(make_spec(matchups)));
}

TEST_CASE("self_play() seats one strategy everywhere")
{
    std::vector<Matchup> matchups = self_play("tictactoe", {}, strategy("minimax"));

    REQUIRE(matchups.size() == 1);
    CHECK(matchups[0].seats.size() == 2);
    CHECK(matchups[0].seats[1].id == "minimax");
}

TEST_CASE("validate() accepts a well-formed spec")
{
    CHECK_NOTHROW(validate(make_spec(round_robin("tictactoe", {}, { strategy("random"), strategy("minimax") }, true))));
}

TEST_CASE("validate() names the matchup for every kind of mistake")
{
    CHECK_THROWS_AS(validate(make_spec({})), ExperimentError);

    ExperimentSpec bad_counts = make_spec({ tictactoe_matchup("random", "random") });
    bad_counts.repeats = 0;
    CHECK_THROWS_AS(validate(bad_counts), ExperimentError);

    CHECK_THROWS_WITH_AS(validate(make_spec({ tictactoe_matchup("random", "nope") })), doctest::Contains("unknown strategy 'nope' in seat 1"), ExperimentError);

    Matchup unknown_game = tictactoe_matchup("random", "random");
    unknown_game.game = "nope";
    CHECK_THROWS_WITH_AS(validate(make_spec({ unknown_game })), doctest::Contains("unknown game 'nope'"), ExperimentError);

    Matchup wrong_seats = tictactoe_matchup("random", "random");
    wrong_seats.seats.push_back(strategy("random"));
    CHECK_THROWS_WITH_AS(validate(make_spec({ wrong_seats })), doctest::Contains("3 seats"), ExperimentError);

    Matchup bad_param = tictactoe_matchup("random", "random");
    bad_param.seats[0].params["depth"] = int64_t{ 2 };
    CHECK_THROWS_WITH_AS(validate(make_spec({ bad_param })), doctest::Contains("depth"), ExperimentError);
}

TEST_CASE("validate() rejects duplicate matchup keys")
{
    Matchup matchup = tictactoe_matchup("random", "random");
    CHECK_THROWS_WITH_AS(validate(make_spec({ matchup, matchup })), doctest::Contains("duplicate"), ExperimentError);

    Matchup labelled = matchup;
    labelled.label = "second";
    CHECK_NOTHROW(validate(make_spec({ matchup, labelled })));
}

TEST_CASE("format_double() ignores the global locale")
{
    std::locale previous = std::locale::global(std::locale(std::locale::classic(), new std::numpunct_byname<char>("C")));
    CHECK(format_double(1.5) == "1.5");
    std::locale::global(previous);
}
