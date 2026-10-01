#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Simulation/ExperimentRunner.h"

namespace oryx
{

constexpr int32_t kResultSchemaVersion = 1;
constexpr const char* kResultFileName = "result.yaml";
constexpr const char* kTrialsFileName = "trials.csv";

// Writes result.yaml (version, spec, metadata, per-matchup summary) and a streamed trials.csv into `directory`.
void save_result(const ExperimentResult& result, const std::filesystem::path& directory);
// Rejects an unknown schema_version, a missing or short trials file, and a spec that no longer matches its recorded hash.
[[nodiscard]] ExperimentResult load_result(const std::filesystem::path& directory);

// Tidy long format: matchup,repeat,metric,value.
void write_trials_csv(const ExperimentResult& result, std::ostream& out);

} // namespace oryx
