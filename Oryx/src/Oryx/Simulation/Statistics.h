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

constexpr double kZ95 = 1.959963984540054;

// Wilson score interval for a rate; {0, 0} for no trials.
[[nodiscard]] Interval wilson_interval(int64_t successes, int64_t trials, double z = kZ95);
// Sample stddev (n - 1) and a normal-approximation CI of the mean.
[[nodiscard]] Summary summarize(const std::vector<double>& samples, double z = kZ95);
// a - b with a Welch-style interval; significant when the interval excludes zero.
[[nodiscard]] Comparison compare(const Summary& a, const Summary& b, double z = kZ95);

// One value per repeat for the matchup, in repeat order.
[[nodiscard]] std::vector<double> metric_series(const ExperimentResult& result, const std::string& matchup, const std::string& metric);
// Sum of every repeat's metrics for the matchup.
[[nodiscard]] Metrics aggregate(const ExperimentResult& result, const std::string& matchup);

// Two-seat matchups only; throws ExperimentError otherwise.
[[nodiscard]] CrossTable cross_table(const ExperimentResult& result);
[[nodiscard]] double score(const CrossTable& table, size_t row, size_t column);
// Bradley-Terry strengths on the Elo scale, centred on 0, one per strategy in table order.
[[nodiscard]] std::vector<double> bradley_terry(const CrossTable& table);

} // namespace oryx
