#include "oxpch.h"
#include "Oryx/Simulation/ExperimentRunner.h"

#include "Oryx/Game/IGame.h"
#include "Oryx/Simulation/SeedSequence.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

namespace
{

constexpr const char* kSeedParam = "seed";

bool declares_seed(const EntryInfo* info)
{
    if (info == nullptr)
    {
        return false;
    }
    for (const ParamSpec& spec : info->schema)
    {
        if (spec.name == kSeedParam)
        {
            return true;
        }
    }
    return false;
}

Params with_derived_seed(const EntryInfo* info, Params params, uint64_t seed)
{
    if (declares_seed(info) && !has_param(params, kSeedParam))
    {
        params[kSeedParam] = static_cast<int64_t>(seed);
    }
    return params;
}

std::string current_timestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

int32_t trial_count(const ExperimentSpec& spec)
{
    return static_cast<int32_t>(spec.matchups.size()) * spec.repeats;
}

std::string trial_id(const TrialResult& trial)
{
    return trial.matchup + '\n' + std::to_string(trial.repeat);
}

} // namespace

Metrics to_metrics(const BatchResult& batch)
{
    Metrics metrics;
    add_metric(metrics, "matches", batch.matches);
    add_metric(metrics, "draws", batch.draws);
    add_metric(metrics, "decisions", static_cast<double>(batch.decisions));
    for (size_t player = 0; player < batch.wins.size(); ++player)
    {
        add_metric(metrics, "wins/" + std::to_string(player), batch.wins[player]);
        add_metric(metrics, "reward/" + std::to_string(player), batch.rewards[static_cast<PlayerId>(player)]);
    }
    return metrics;
}

BatchResult to_batch_result(const Metrics& metrics, size_t player_count)
{
    BatchResult batch;
    batch.matches = static_cast<int32_t>(get_metric(metrics, "matches"));
    batch.draws = static_cast<int32_t>(get_metric(metrics, "draws"));
    batch.decisions = static_cast<int64_t>(get_metric(metrics, "decisions"));
    batch.wins.assign(player_count, 0);
    batch.rewards = Rewards<double>(player_count);
    for (size_t player = 0; player < player_count; ++player)
    {
        batch.wins[player] = static_cast<int32_t>(get_metric(metrics, "wins/" + std::to_string(player)));
        batch.rewards[static_cast<PlayerId>(player)] = get_metric(metrics, "reward/" + std::to_string(player));
    }
    return batch;
}

TrialResult run_trial(const ExperimentSpec& spec, size_t matchup_index, int32_t repeat)
{
    if (matchup_index >= spec.matchups.size())
    {
        throw ExperimentError("run_trial: matchup index " + std::to_string(matchup_index) + " is out of range");
    }

    const Matchup& matchup = spec.matchups[matchup_index];
    std::string key = matchup_key(matchup);
    std::string where = "matchup '" + key + "': ";
    uint32_t repeat_index = static_cast<uint32_t>(repeat);

    uint64_t game_seed = derive_seed(spec.master_seed, key, SeedRole::Game, 0, repeat_index);
    UniquePtr<IGame> game = create_game(matchup.game, with_derived_seed(GameRegistry::info(matchup.game), matchup.game_params, game_seed));
    if (game == nullptr)
    {
        throw ExperimentError(where + "unknown game '" + matchup.game + "'");
    }

    std::vector<UniquePtr<IStrategy>> owned;
    SmallVector<IStrategy*, 2> strategies;
    for (size_t seat = 0; seat < matchup.seats.size(); ++seat)
    {
        const StrategySpec& strategy_spec = matchup.seats[seat];
        uint64_t seed = derive_seed(spec.master_seed, key, SeedRole::Strategy, static_cast<uint32_t>(seat), repeat_index);
        UniquePtr<IStrategy> strategy = StrategyRegistry::create(strategy_spec.id, with_derived_seed(StrategyRegistry::info(strategy_spec.id), strategy_spec.params, seed));
        if (strategy == nullptr)
        {
            throw ExperimentError(where + "unknown strategy '" + strategy_spec.id + "' in seat " + std::to_string(seat));
        }
        strategies.push_back(strategy.get());
        owned.push_back(std::move(strategy));
    }

    BatchRunner runner(*game, std::move(strategies));
    TrialResult trial;
    trial.matchup = key;
    trial.repeat = repeat;
    trial.metrics = to_metrics(runner.run(spec.matches_per_trial));
    return trial;
}

ExperimentResult run_experiment(const ExperimentSpec& spec, const RunOptions& options)
{
    validate(spec);

    ExperimentResult result;
    result.spec = spec;
    result.metadata.build = build_info();
    result.metadata.spec_hash = spec_hash(spec);
    result.metadata.master_seed = spec.master_seed;
    if (options.record_timestamp)
    {
        result.metadata.timestamp = current_timestamp();
    }

    int32_t total = trial_count(spec);
    result.trials.reserve(static_cast<size_t>(total));
    for (size_t matchup = 0; matchup < spec.matchups.size(); ++matchup)
    {
        for (int32_t repeat = 0; repeat < spec.repeats; ++repeat)
        {
            if (options.cancel != nullptr && options.cancel->load())
            {
                return result;
            }
            result.trials.push_back(run_trial(spec, matchup, repeat));
            if (options.progress)
            {
                options.progress(static_cast<int32_t>(result.trials.size()), total);
            }
        }
    }
    return result;
}

bool is_complete(const ExperimentResult& result)
{
    return static_cast<int32_t>(result.trials.size()) == trial_count(result.spec);
}

ExperimentResult rerun(const ExperimentResult& result, const RunOptions& options)
{
    return run_experiment(result.spec, options);
}

bool trials_equal(const ExperimentResult& a, const ExperimentResult& b)
{
    if (a.trials.size() != b.trials.size())
    {
        return false;
    }
    std::map<std::string, const Metrics*> by_id;
    for (const TrialResult& trial : a.trials)
    {
        by_id[trial_id(trial)] = &trial.metrics;
    }
    for (const TrialResult& trial : b.trials)
    {
        std::map<std::string, const Metrics*>::const_iterator found = by_id.find(trial_id(trial));
        if (found == by_id.end() || found->second->values != trial.metrics.values)
        {
            return false;
        }
    }
    return true;
}

} // namespace oryx
