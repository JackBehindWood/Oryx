#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Metrics.h"
#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Simulation/BuildInfo.h"
#include "Oryx/Simulation/Experiment.h"

namespace oryx
{

struct TrialResult
{
    std::string matchup;
    int32_t repeat = 0;
    Metrics metrics;
};

struct Metadata
{
    BuildInfo build;
    uint64_t spec_hash = 0;
    uint64_t master_seed = 0;
    // Empty unless RunOptions::record_timestamp, so a rerun can be byte-identical.
    std::string timestamp;
};

struct ExperimentResult
{
    ExperimentSpec spec;
    Metadata metadata;
    std::vector<TrialResult> trials;
};

struct RunOptions
{
    std::function<void(int32_t completed, int32_t total)> progress;
    const std::atomic<bool>* cancel = nullptr;
    bool record_timestamp = false;
    // Adds each strategy's published diagnostics ("minimax/nodes") to the trial metrics; rerun must pass the same option to reproduce them.
    bool collect_diagnostics = false;
};

[[nodiscard]] Metrics to_metrics(const BatchResult& batch);
[[nodiscard]] BatchResult to_batch_result(const Metrics& metrics, size_t player_count);

// A pure job: fresh game and strategies from the registries, seeds derived from the matchup key and repeat.
[[nodiscard]] TrialResult run_trial(const ExperimentSpec& spec, size_t matchup_index, int32_t repeat, const RunOptions& options = {});

// Validates first. A cancelled run returns the trials finished so far; see is_complete().
[[nodiscard]] ExperimentResult run_experiment(const ExperimentSpec& spec, const RunOptions& options = {});

[[nodiscard]] bool is_complete(const ExperimentResult& result);
// Reruns the stored spec; names any id the registries no longer know.
[[nodiscard]] ExperimentResult rerun(const ExperimentResult& result, const RunOptions& options = {});
// Compares trials by matchup key and repeat, ignoring order and metadata.
[[nodiscard]] bool trials_equal(const ExperimentResult& a, const ExperimentResult& b);

} // namespace oryx
