#include "oxpch.h"
#include "Oryx/Simulation/Statistics.h"

namespace oryx
{

Interval wilson_interval(int64_t successes, int64_t trials, double z)
{
    if (trials <= 0)
    {
        return {};
    }
    double n = static_cast<double>(trials);
    double p = static_cast<double>(successes) / n;
    double z2 = z * z;
    double denominator = 1.0 + z2 / n;
    double centre = (p + z2 / (2.0 * n)) / denominator;
    double margin = z * std::sqrt(p * (1.0 - p) / n + z2 / (4.0 * n * n)) / denominator;
    return { std::max(0.0, centre - margin), std::min(1.0, centre + margin) };
}

double t_critical_95(int64_t degrees_of_freedom)
{
    constexpr double kTable[] = { 12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228, 2.201, 2.179, 2.160, 2.145, 2.131,
                                  2.120, 2.110, 2.101, 2.093, 2.086, 2.080, 2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042 };
    constexpr int64_t kTableSize = static_cast<int64_t>(sizeof(kTable) / sizeof(kTable[0]));
    if (degrees_of_freedom < 1)
    {
        return kTable[0];
    }
    return degrees_of_freedom <= kTableSize ? kTable[degrees_of_freedom - 1] : kZ95;
}

Summary summarize(const std::vector<double>& samples)
{
    Summary summary;
    summary.count = static_cast<int64_t>(samples.size());
    if (samples.empty())
    {
        return summary;
    }

    double sum = 0.0;
    for (double sample : samples)
    {
        sum += sample;
    }
    summary.mean = sum / static_cast<double>(samples.size());

    if (samples.size() > 1)
    {
        double squares = 0.0;
        for (double sample : samples)
        {
            squares += (sample - summary.mean) * (sample - summary.mean);
        }
        summary.stddev = std::sqrt(squares / static_cast<double>(samples.size() - 1));
        summary.ci_half_width = t_critical_95(summary.count - 1) * summary.stddev / std::sqrt(static_cast<double>(samples.size()));
    }
    return summary;
}

Comparison compare(const Summary& a, const Summary& b)
{
    Comparison comparison;
    comparison.difference = a.mean - b.mean;
    if (a.count < 2 || b.count < 2)
    {
        return comparison;
    }

    double variance_a = a.stddev * a.stddev / static_cast<double>(a.count);
    double variance_b = b.stddev * b.stddev / static_cast<double>(b.count);
    double variance = variance_a + variance_b;
    double welch_denominator = variance_a * variance_a / static_cast<double>(a.count - 1) + variance_b * variance_b / static_cast<double>(b.count - 1);
    int64_t degrees = welch_denominator > 0.0 ? static_cast<int64_t>(variance * variance / welch_denominator) : a.count + b.count - 2;
    double margin = t_critical_95(degrees) * std::sqrt(variance);
    comparison.interval = { comparison.difference - margin, comparison.difference + margin };
    comparison.significant = comparison.interval.lower > 0.0 || comparison.interval.upper < 0.0;
    return comparison;
}

namespace
{

void require_matchup(const ExperimentResult& result, const std::string& matchup)
{
    for (const Matchup& candidate : result.spec.matchups)
    {
        if (matchup_key(candidate) == matchup)
        {
            return;
        }
    }
    throw ExperimentError("no matchup '" + matchup + "' in experiment '" + result.spec.name + "'");
}

} // namespace

std::vector<double> metric_series(const ExperimentResult& result, const std::string& matchup, const std::string& metric)
{
    require_matchup(result, matchup);
    std::vector<double> series;
    for (const TrialResult& trial : result.trials)
    {
        if (trial.matchup == matchup)
        {
            series.push_back(get_metric(trial.metrics, metric));
        }
    }
    return series;
}

Metrics aggregate(const ExperimentResult& result, const std::string& matchup)
{
    require_matchup(result, matchup);
    Metrics total;
    for (const TrialResult& trial : result.trials)
    {
        if (trial.matchup == matchup)
        {
            merge(total, trial.metrics);
        }
    }
    return total;
}

std::map<std::string, Metrics> aggregate_all(const ExperimentResult& result)
{
    std::map<std::string, Metrics> totals;
    for (const Matchup& matchup : result.spec.matchups)
    {
        totals[matchup_key(matchup)];
    }
    for (const TrialResult& trial : result.trials)
    {
        std::map<std::string, Metrics>::iterator found = totals.find(trial.matchup);
        if (found != totals.end())
        {
            merge(found->second, trial.metrics);
        }
    }
    return totals;
}

CrossTable cross_table(const ExperimentResult& result)
{
    CrossTable table;
    std::map<std::string, size_t> index;
    auto index_of = [&](const StrategySpec& strategy) -> size_t
    {
        std::string key = strategy_key(strategy);
        std::map<std::string, size_t>::const_iterator found = index.find(key);
        if (found != index.end())
        {
            return found->second;
        }
        index.emplace(key, table.strategies.size());
        table.strategies.push_back(key);
        return table.strategies.size() - 1;
    };

    std::vector<std::pair<size_t, size_t>> seats;
    for (const Matchup& matchup : result.spec.matchups)
    {
        if (matchup.seats.size() != 2)
        {
            throw ExperimentError("matchup '" + matchup_key(matchup) + "': cross tables and ratings cover two-seat matchups only");
        }
        seats.emplace_back(index_of(matchup.seats[0]), index_of(matchup.seats[1]));
    }

    std::map<std::string, Metrics> totals = aggregate_all(result);
    size_t count = table.strategies.size();
    table.points.assign(count * count, 0.0);
    table.games.assign(count * count, 0.0);
    for (size_t matchup = 0; matchup < seats.size(); ++matchup)
    {
        const Metrics& total = totals[matchup_key(result.spec.matchups[matchup])];
        double games = get_metric(total, "matches");
        double draws = get_metric(total, "draws");
        double first = get_metric(total, "wins/0") + 0.5 * draws;
        double second = get_metric(total, "wins/1") + 0.5 * draws;
        size_t row = seats[matchup].first;
        size_t column = seats[matchup].second;
        table.points[row * count + column] += first;
        table.points[column * count + row] += second;
        table.games[row * count + column] += games;
        table.games[column * count + row] += games;
    }
    return table;
}

double score(const CrossTable& table, size_t row, size_t column)
{
    size_t count = table.strategies.size();
    double games = table.games[row * count + column];
    return games == 0.0 ? 0.0 : table.points[row * count + column] / games;
}

std::vector<double> bradley_terry(const CrossTable& table)
{
    size_t count = table.strategies.size();
    std::vector<double> strength(count, 1.0);
    if (count == 0)
    {
        return strength;
    }

    // One virtual drawn game per played pair keeps an unbeaten or winless strategy's strength finite.
    std::vector<double> wins = table.points;
    std::vector<double> games = table.games;
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = 0; j < count; ++j)
        {
            if (i != j && games[i * count + j] > 0.0)
            {
                wins[i * count + j] += 0.5;
                games[i * count + j] += 1.0;
            }
        }
    }

    constexpr int32_t kMaxIterations = 10000;
    constexpr double kTolerance = 1e-12;
    for (int32_t iteration = 0; iteration < kMaxIterations; ++iteration)
    {
        std::vector<double> next(count, 1.0);
        double change = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            double won = 0.0;
            double expected = 0.0;
            for (size_t j = 0; j < count; ++j)
            {
                if (i != j && games[i * count + j] > 0.0)
                {
                    won += wins[i * count + j];
                    expected += games[i * count + j] / (strength[i] + strength[j]);
                }
            }
            next[i] = expected > 0.0 ? won / expected : strength[i];
        }

        double log_sum = 0.0;
        for (double value : next)
        {
            log_sum += std::log(value);
        }
        double scale = std::exp(log_sum / static_cast<double>(count));
        for (size_t i = 0; i < count; ++i)
        {
            next[i] /= scale;
            change = std::max(change, std::abs(next[i] - strength[i]));
        }
        strength = std::move(next);
        if (change < kTolerance)
        {
            break;
        }
    }

    std::vector<double> ratings(count);
    for (size_t i = 0; i < count; ++i)
    {
        ratings[i] = 400.0 * std::log10(strength[i]);
    }
    return ratings;
}

} // namespace oryx
