#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

StrategySpec strategy(const std::string& id)
{
    return StrategySpec{ id, {} };
}

ExperimentSpec make_spec()
{
    ExperimentSpec spec;
    spec.name = "runner";
    spec.matchups = round_robin("tictactoe", {}, { strategy("random"), strategy("first-legal"), strategy("tictactoe/heuristic") }, true);
    spec.matches_per_trial = 10;
    spec.repeats = 2;
    spec.master_seed = 1234;
    return spec;
}

std::filesystem::path temp_directory(const std::string& name)
{
    std::filesystem::path directory = std::filesystem::temp_directory_path() / ("oryx-experiment-" + name);
    std::filesystem::remove_all(directory);
    return directory;
}

std::string read_text(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

void write_text(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out(path);
    out << text;
}

std::string replace_first(std::string text, const std::string& from, const std::string& to)
{
    size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}

} // namespace

TEST_CASE("run_experiment() runs every matchup for every repeat")
{
    ExperimentSpec spec = make_spec();

    ExperimentResult result = run_experiment(spec);

    CHECK(result.trials.size() == 12);
    CHECK(is_complete(result));
    for (const TrialResult& trial : result.trials)
    {
        CHECK(get_metric(trial.metrics, "matches") == 10.0);
        CHECK(get_metric(trial.metrics, "wins/0") + get_metric(trial.metrics, "wins/1") + get_metric(trial.metrics, "draws") == 10.0);
    }
    CHECK(result.metadata.spec_hash == spec_hash(spec));
    CHECK(result.metadata.timestamp.empty());
}

TEST_CASE("run_experiment() rejects an invalid spec before running anything")
{
    ExperimentSpec spec = make_spec();
    spec.matchups[0].seats[0].id = "nope";

    CHECK_THROWS_AS(run_experiment(spec), ExperimentError);
}

TEST_CASE("The same spec twice gives identical trials")
{
    ExperimentSpec spec = make_spec();

    CHECK(trials_equal(run_experiment(spec), run_experiment(spec)));
}

TEST_CASE("A different master seed changes seeded strategies' results")
{
    ExperimentSpec spec = make_spec();
    spec.matchups = self_play("tictactoe", {}, strategy("random"));
    spec.matches_per_trial = 200;
    ExperimentSpec other = spec;
    other.master_seed = spec.master_seed + 1;

    CHECK_FALSE(trials_equal(run_experiment(spec), run_experiment(other)));
}

TEST_CASE("Reordering matchups leaves every matchup's results unchanged")
{
    ExperimentSpec spec = make_spec();
    ExperimentSpec reversed = spec;
    std::reverse(reversed.matchups.begin(), reversed.matchups.end());

    CHECK(trials_equal(run_experiment(spec), run_experiment(reversed)));
}

TEST_CASE("Adding a matchup leaves existing matchups' results unchanged")
{
    ExperimentSpec spec = make_spec();
    ExperimentSpec extended = spec;
    extended.matchups.push_back(self_play("tictactoe", {}, strategy("random"))[0]);
    ExperimentResult base = run_experiment(spec);
    ExperimentResult more = run_experiment(extended);

    for (const TrialResult& trial : base.trials)
    {
        CHECK(aggregate(base, trial.matchup).values == aggregate(more, trial.matchup).values);
    }
}

TEST_CASE("Pinning a seed param overrides the derived one")
{
    ExperimentSpec spec = make_spec();
    spec.matchups = self_play("tictactoe", {}, StrategySpec{ "random", { { "seed", int64_t{ 5 } } } });
    spec.matches_per_trial = 200;
    spec.repeats = 2;

    ExperimentResult result = run_experiment(spec);

    CHECK(result.trials[0].metrics.values == result.trials[1].metrics.values);
}

TEST_CASE("run_experiment() stops on cancel and reports progress")
{
    ExperimentSpec spec = make_spec();
    std::atomic<bool> cancel{ false };
    int32_t reported = 0;
    RunOptions options;
    options.cancel = &cancel;
    options.progress = [&](int32_t completed, int32_t total)
    {
        reported = completed;
        CHECK(total == 12);
        if (completed == 3)
        {
            cancel = true;
        }
    };

    ExperimentResult result = run_experiment(spec, options);

    CHECK(result.trials.size() == 3);
    CHECK(reported == 3);
    CHECK_FALSE(is_complete(result));
}

TEST_CASE("record_timestamp stamps the metadata")
{
    RunOptions options;
    options.record_timestamp = true;

    CHECK_FALSE(run_experiment(make_spec(), options).metadata.timestamp.empty());
}

TEST_CASE("to_batch_result() inverts to_metrics()")
{
    BatchResult batch;
    batch.matches = 7;
    batch.wins.assign(2, 0);
    batch.wins[0] = 4;
    batch.wins[1] = 2;
    batch.draws = 1;
    batch.rewards = Rewards<double>(2);
    batch.rewards[0] = 3.0;
    batch.rewards[1] = -3.0;
    batch.decisions = 40;

    BatchResult back = to_batch_result(to_metrics(batch), 2);

    CHECK(back.matches == 7);
    CHECK(back.wins == batch.wins);
    CHECK(back.draws == 1);
    CHECK(back.rewards[1] == doctest::Approx(-3.0));
    CHECK(back.decisions == 40);
}

TEST_CASE("save_result() then load_result() round-trips, and rerun() reproduces the counts")
{
    ExperimentSpec spec = make_spec();
    spec.matchups.erase(spec.matchups.begin());
    spec.matchups[0].seats[1].params["seed"] = int64_t{ 9 };
    spec.name = "round, \"trip\"";
    std::filesystem::path directory = temp_directory("roundtrip");
    ExperimentResult result = run_experiment(spec);

    save_result(result, directory);
    ExperimentResult loaded = load_result(directory);

    CHECK(loaded.spec.name == spec.name);
    CHECK(loaded.spec.master_seed == spec.master_seed);
    CHECK(loaded.metadata.spec_hash == result.metadata.spec_hash);
    CHECK(loaded.metadata.build.version == result.metadata.build.version);
    CHECK(trials_equal(result, loaded));
    CHECK(trials_equal(loaded, rerun(loaded)));
}

TEST_CASE("Every param type survives a save/load round trip exactly")
{
    ExperimentResult result;
    result.spec.name = "types";
    Matchup matchup;
    matchup.game = "g";
    matchup.game_params = { { "flag", true }, { "count", int64_t{ -7 } }, { "rate", 0.1 + 0.2 }, { "mode", std::string("a, \"b\"") } };
    matchup.seats = { StrategySpec{ "s", { { "rate", 1.0 / 3.0 } } } };
    result.spec.matchups = { matchup };
    result.spec.master_seed = 18446744073709551615ULL;
    result.metadata.spec_hash = spec_hash(result.spec);
    result.metadata.master_seed = result.spec.master_seed;
    std::filesystem::path directory = temp_directory("types");

    save_result(result, directory);
    ExperimentResult loaded = load_result(directory);

    CHECK(canonical_string(loaded.spec) == canonical_string(result.spec));
    CHECK(loaded.spec.master_seed == 18446744073709551615ULL);
    CHECK(std::get<double>(loaded.spec.matchups[0].game_params.at("rate")) == 0.1 + 0.2);
    CHECK(std::get<double>(loaded.spec.matchups[0].seats[0].params.at("rate")) == 1.0 / 3.0);
}

TEST_CASE("Saved results are byte-identical for the same spec")
{
    ExperimentSpec spec = make_spec();
    std::filesystem::path first = temp_directory("bytes-a");
    std::filesystem::path second = temp_directory("bytes-b");

    save_result(run_experiment(spec), first);
    save_result(run_experiment(spec), second);

    for (const char* name : { k_result_file_name, k_trials_file_name })
    {
        std::ifstream a(first / name);
        std::ifstream b(second / name);
        std::stringstream text_a;
        std::stringstream text_b;
        text_a << a.rdbuf();
        text_b << b.rdbuf();
        CHECK(text_a.str() == text_b.str());
    }
}

TEST_CASE("load_result() rejects a missing directory, an unknown schema version, a short trials file and an edited spec")
{
    ExperimentSpec spec = make_spec();
    std::filesystem::path directory = temp_directory("rejects");
    save_result(run_experiment(spec), directory);

    CHECK_THROWS_AS(load_result(directory / "missing"), ExperimentError);

    auto read = [](const std::filesystem::path& path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    };
    auto write = [](const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream out(path);
        out << text;
    };
    auto replace = [](std::string text, const std::string& from, const std::string& to)
    {
        size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        return text.replace(at, from.size(), to);
    };

    std::string yaml = read(directory / k_result_file_name);
    std::string csv = read(directory / k_trials_file_name);

    write(directory / k_result_file_name, replace(yaml, "schema_version: 1", "schema_version: 99"));
    CHECK_THROWS_WITH_AS(load_result(directory), doctest::Contains("schema_version 99"), ExperimentError);

    write(directory / k_result_file_name, replace(yaml, "repeats: 2", "repeats: 3"));
    CHECK_THROWS_WITH_AS(load_result(directory), doctest::Contains("spec_hash"), ExperimentError);

    write(directory / k_result_file_name, yaml);
    write(directory / k_trials_file_name, csv.substr(0, csv.size() / 2));
    CHECK_THROWS_AS(load_result(directory), ExperimentError);
}

TEST_CASE("load_result() reports a malformed result.yaml as an ExperimentError")
{
    std::filesystem::path directory = temp_directory("bad-yaml");
    save_result(run_experiment(make_spec()), directory);
    std::string yaml = read_text(directory / k_result_file_name);

    SUBCASE("not yaml at all")
    {
        write_text(directory / k_result_file_name, "{ unclosed: [");
    }
    SUBCASE("empty file")
    {
        write_text(directory / k_result_file_name, "");
    }
    SUBCASE("missing schema_version")
    {
        write_text(directory / k_result_file_name, replace_first(yaml, "schema_version", "schema_versio"));
    }
    SUBCASE("missing metadata")
    {
        write_text(directory / k_result_file_name, replace_first(yaml, "metadata:", "metadat:"));
    }
    SUBCASE("non-numeric master seed")
    {
        write_text(directory / k_result_file_name, replace_first(yaml, "master_seed: 1234", "master_seed: 12x4"));
    }
    SUBCASE("master seed beyond 64 bits")
    {
        write_text(directory / k_result_file_name, replace_first(yaml, "master_seed: 1234", "master_seed: 99999999999999999999999"));
    }
    SUBCASE("negative master seed")
    {
        write_text(directory / k_result_file_name, replace_first(yaml, "master_seed: 1234", "master_seed: -5"));
    }
    CHECK_THROWS_AS(load_result(directory), ExperimentError);
}

TEST_CASE("load_result() reports a malformed trials.csv as an ExperimentError")
{
    std::filesystem::path directory = temp_directory("bad-csv");
    save_result(run_experiment(make_spec()), directory);
    std::string csv = read_text(directory / k_trials_file_name);
    std::string first_row = csv.substr(csv.find('\n') + 1, csv.find('\n', csv.find('\n') + 1) - csv.find('\n') - 1);
    std::string matchup = first_row.substr(0, first_row.find(','));

    SUBCASE("empty file")
    {
        write_text(directory / k_trials_file_name, "");
    }
    SUBCASE("header only")
    {
        write_text(directory / k_trials_file_name, "matchup,repeat,metric,value\n");
    }
    SUBCASE("wrong column count")
    {
        write_text(directory / k_trials_file_name, csv + "a,b\n");
    }
    SUBCASE("repeat with trailing garbage")
    {
        write_text(directory / k_trials_file_name, replace_first(csv, "," + std::string("0,"), ",0x,"));
    }
    SUBCASE("value with trailing garbage")
    {
        write_text(directory / k_trials_file_name, replace_first(csv, first_row, first_row + "z"));
    }
    SUBCASE("matchup missing from the spec")
    {
        write_text(directory / k_trials_file_name, replace_first(csv, matchup, "no such matchup"));
    }
    SUBCASE("repeat outside the spec")
    {
        write_text(directory / k_trials_file_name, replace_first(csv, matchup + ",0,", matchup + ",7,"));
    }
    SUBCASE("metric listed twice")
    {
        write_text(directory / k_trials_file_name, csv + first_row + "\n");
    }
    CHECK_THROWS_AS(load_result(directory), ExperimentError);
}

TEST_CASE("load_result() reads a trials.csv with CRLF line endings")
{
    ExperimentResult result = run_experiment(make_spec());
    std::filesystem::path directory = temp_directory("crlf");
    save_result(result, directory);

    std::string crlf;
    for (char character : read_text(directory / k_trials_file_name))
    {
        crlf += character == '\n' ? "\r\n" : std::string(1, character);
    }
    write_text(directory / k_trials_file_name, crlf);
    CHECK(trials_equal(load_result(directory), result));
}

TEST_CASE("A matchup label with a comma or quote survives the CSV round trip")
{
    ExperimentSpec spec = make_spec();
    spec.matchups.resize(1);
    spec.matchups[0].label = "a, \"quoted\" label";
    ExperimentResult result = run_experiment(spec);
    std::filesystem::path directory = temp_directory("label");
    save_result(result, directory);
    ExperimentResult loaded = load_result(directory);
    CHECK(loaded.trials[0].matchup == spec.matchups[0].label);
    CHECK(trials_equal(loaded, result));
}

TEST_CASE("save_result() writes nothing when a label cannot be stored in CSV")
{
    ExperimentSpec spec = make_spec();
    spec.matchups.resize(1);
    spec.matchups[0].label = "two\nlines";
    ExperimentResult result = run_experiment(spec);
    std::filesystem::path directory = temp_directory("newline");
    CHECK_THROWS_AS(save_result(result, directory), ExperimentError);
    CHECK_FALSE(std::filesystem::exists(directory / k_result_file_name));
}

TEST_CASE("save_result() reports an uncreatable directory as an ExperimentError")
{
    std::filesystem::path blocker = temp_directory("blocker");
    write_text(blocker, "a file, not a directory");
    CHECK_THROWS_AS(save_result(run_experiment(make_spec()), blocker / "inside"), ExperimentError);
    std::filesystem::remove(blocker);
}

TEST_CASE("A cancelled, incomplete result round-trips with its trials")
{
    ExperimentSpec spec = make_spec();
    std::atomic<bool> cancel{ false };
    RunOptions options;
    options.cancel = &cancel;
    options.progress = [&](int32_t completed, int32_t) { cancel = completed >= 3; };
    ExperimentResult result = run_experiment(spec, options);
    REQUIRE_FALSE(is_complete(result));

    std::filesystem::path directory = temp_directory("incomplete");
    save_result(result, directory);
    ExperimentResult loaded = load_result(directory);
    CHECK(loaded.trials.size() == 3);
    CHECK(trials_equal(loaded, result));
}

TEST_CASE("A result with many trials saves and loads")
{
    ExperimentSpec spec = make_spec();
    spec.matchups.resize(1);
    spec.matches_per_trial = 1;
    spec.repeats = 5000;
    ExperimentResult result = run_experiment(spec);
    std::filesystem::path directory = temp_directory("many");
    save_result(result, directory);
    ExperimentResult loaded = load_result(directory);
    CHECK(loaded.trials.size() == 5000);
    CHECK(trials_equal(loaded, result));
}

TEST_CASE("rerun() names a game or strategy that is no longer registered")
{
    ExperimentResult result = run_experiment(make_spec());
    result.spec.matchups[0].seats[1].id = "gone";

    CHECK_THROWS_WITH_AS(rerun(result), doctest::Contains("gone"), ExperimentError);
}

TEST_CASE("wilson_interval() brackets the rate and handles the edges")
{
    Interval half = wilson_interval(50, 100);
    CHECK(half.lower < 0.5);
    CHECK(half.upper > 0.5);
    CHECK(half.lower == doctest::Approx(0.4038).epsilon(0.001));
    CHECK(half.upper == doctest::Approx(0.5962).epsilon(0.001));

    Interval none = wilson_interval(0, 20);
    CHECK(none.lower == 0.0);
    CHECK(none.upper > 0.0);
    CHECK(wilson_interval(0, 0).upper == 0.0);
    CHECK(wilson_interval(10, 10).upper == doctest::Approx(1.0));
}

TEST_CASE("summarize() and compare() report mean, spread and significance")
{
    Summary a = summarize({ 1.0, 2.0, 3.0, 4.0 });
    CHECK(a.count == 4);
    CHECK(a.mean == doctest::Approx(2.5));
    CHECK(a.stddev == doctest::Approx(1.2909944));
    CHECK(summarize({ 5.0 }).stddev == 0.0);
    CHECK(summarize({}).count == 0);

    Summary far = summarize({ 10.0, 10.1, 9.9, 10.0 });
    Summary near = summarize({ 1.0, 1.1, 0.9, 1.0 });
    Comparison apart = compare(far, near);
    CHECK(apart.difference == doctest::Approx(9.0));
    CHECK(apart.significant);
    CHECK_FALSE(compare(a, a).significant);
}

TEST_CASE("Intervals use Student-t critical values for few repeats")
{
    CHECK(t_critical_95(1) == doctest::Approx(12.706));
    CHECK(t_critical_95(4) == doctest::Approx(2.776));
    CHECK(t_critical_95(30) == doctest::Approx(2.042));
    CHECK(t_critical_95(31) == doctest::Approx(k_z95));
    Summary five = summarize({ 1.0, 2.0, 3.0, 4.0, 5.0 });
    CHECK(five.ci_half_width == doctest::Approx(2.776 * five.stddev / std::sqrt(5.0)));
}

TEST_CASE("compare() is never significant with fewer than two repeats on a side")
{
    CHECK_FALSE(compare(summarize({ 10.0 }), summarize({ 1.0 })).significant);
    CHECK_FALSE(compare(summarize({ 10.0, 10.1 }), summarize({ 1.0 })).significant);
    CHECK(compare(summarize({ 10.0 }), summarize({ 1.0 })).difference == doctest::Approx(9.0));
}

TEST_CASE("series and aggregates reject a matchup that is not in the spec")
{
    ExperimentResult result = run_experiment(make_spec());
    CHECK_THROWS_AS(metric_series(result, "typo", "wins/0"), ExperimentError);
    CHECK_THROWS_AS(aggregate(result, "typo"), ExperimentError);
}

TEST_CASE("aggregate_all() equals aggregate() per matchup")
{
    ExperimentResult result = run_experiment(make_spec());
    std::map<std::string, Metrics> all = aggregate_all(result);
    CHECK(all.size() == result.spec.matchups.size());
    for (const Matchup& matchup : result.spec.matchups)
    {
        CHECK(all[matchup_key(matchup)].values == aggregate(result, matchup_key(matchup)).values);
    }
}

TEST_CASE("run_trial() rejects a repeat outside the spec")
{
    ExperimentSpec spec = make_spec();
    CHECK_THROWS_AS(run_trial(spec, 0, -1), ExperimentError);
    CHECK_THROWS_AS(run_trial(spec, 0, spec.repeats), ExperimentError);
    CHECK_NOTHROW(run_trial(spec, 0, spec.repeats - 1));
}

TEST_CASE("cross_table() and bradley_terry() rank a stronger strategy above a weaker one")
{
    ExperimentSpec spec;
    spec.name = "table";
    spec.matchups = round_robin("tictactoe", {}, { strategy("random"), strategy("minimax") }, true);
    spec.matches_per_trial = 6;
    spec.master_seed = 3;

    ExperimentResult result = run_experiment(spec);
    CrossTable table = cross_table(result);
    std::vector<double> ratings = bradley_terry(table);

    REQUIRE(table.strategies.size() == 2);
    CHECK(table.strategies[0] == "random");
    CHECK(score(table, 1, 0) > score(table, 0, 1));
    CHECK(score(table, 0, 1) + score(table, 1, 0) == doctest::Approx(1.0));
    CHECK(ratings[1] > ratings[0]);
    CHECK(ratings[0] + ratings[1] == doctest::Approx(0.0).epsilon(1e-6));
}

TEST_CASE("cross_table() rejects matchups that are not two-seat")
{
    ExperimentResult result;
    result.spec.matchups = self_play("tictactoe", {}, strategy("random"), 3);

    CHECK_THROWS_AS(cross_table(result), ExperimentError);
}

TEST_CASE("build_info() describes the build")
{
    BuildInfo info = build_info();

    CHECK(info.version == "0.1.0");
    CHECK_FALSE(info.profile.empty());
    CHECK_FALSE(info.compiler.empty());
    CHECK_FALSE(info.platform.empty());
}
