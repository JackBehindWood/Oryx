#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Metrics.h"
#include "Oryx/Simulation/ExperimentRunner.h"

namespace oryx
{

struct Interval
{
    double lower = 0.0;
    double upper = 0.0;
};

struct Summary
{
    int64_t count = 0;
    double mean = 0.0;
    double stddev = 0.0;
    double ci_half_width = 0.0;
};

struct Comparison
{
    double difference = 0.0;
    Interval interval;
    bool significant = false;
};

// Strategies are rows/columns; `points` counts a win as 1 and a draw as 0.5 for the row strategy against the column one.
struct CrossTable
{
    std::vector<std::string> strategies;
    std::vector<double> points;
    std::vector<double> games;
};

constexpr double k_z95 = 1.959963984540054;

// Wilson score interval for a rate; {0, 0} for no trials.
[[nodiscard]] Interval wilson_interval(int64_t successes, int64_t trials, double z = k_z95);
// Two-sided 95% Student-t critical value; the normal one beyond 30 degrees of freedom.
[[nodiscard]] double t_critical_95(int64_t degrees_of_freedom);
// Sample stddev (n - 1) and a Student-t CI of the mean.
[[nodiscard]] Summary summarize(const std::vector<double>& samples);
// a - b with a Welch interval; significant when the interval excludes zero, never with fewer than two repeats on either side.
[[nodiscard]] Comparison compare(const Summary& a, const Summary& b);

// One value per repeat for the matchup, in repeat order; throws ExperimentError for a matchup not in the spec.
[[nodiscard]] std::vector<double> metric_series(const ExperimentResult& result, const std::string& matchup, const std::string& metric);
// Sum of every repeat's metrics for the matchup; throws ExperimentError for a matchup not in the spec.
[[nodiscard]] Metrics aggregate(const ExperimentResult& result, const std::string& matchup);
// aggregate() for every matchup in one pass over the trials, keyed by matchup key.
[[nodiscard]] std::map<std::string, Metrics> aggregate_all(const ExperimentResult& result);

// Two-seat matchups only; throws ExperimentError otherwise.
[[nodiscard]] CrossTable cross_table(const ExperimentResult& result);
[[nodiscard]] double score(const CrossTable& table, size_t row, size_t column);
// Bradley-Terry strengths on the Elo scale, centred on 0, one per strategy in table order.
[[nodiscard]] std::vector<double> bradley_terry(const CrossTable& table);

} // namespace oryx
