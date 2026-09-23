#pragma once

#include <pybind11/pybind11.h>

#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx::python
{

// What simulate() ran, kept for reproducibility; there is no metadata for a hand-built BatchRunner.
struct RunMetadata
{
    std::string game;
    std::vector<std::string> strategies;
    int32_t games = 0;
    bool seeded = false;
    int64_t seed = 0;
};

// What the Python BatchResult wraps: the counts plus the run's metadata when there is one.
struct PyBatchResult
{
    BatchResult counts;
    RunMetadata metadata;
    bool has_metadata = false;
};

using Strategies = std::vector<SharedPtr<IStrategy>>;

[[nodiscard]] Strategies resolve_strategies(const pybind11::sequence& specs);
// Strategies created by name that declare a seed get seed + seat_index; instances are the caller's to seed.
[[nodiscard]] Strategies resolve_seeded_strategies(const pybind11::sequence& specs, const pybind11::object& seed);
[[nodiscard]] std::vector<std::string> strategy_labels(const pybind11::sequence& specs);
[[nodiscard]] RunMetadata make_metadata(const IGame& game, const pybind11::sequence& strategies, int32_t games, const pybind11::object& seed);

[[nodiscard]] std::vector<IStrategy*> raw_pointers(const Strategies& strategies);
[[nodiscard]] SmallVector<IStrategy*, 2> to_small_vector(const std::vector<IStrategy*>& strategies);

// Throws unless the strategies can play the game; returns whether the GIL has to stay held while they do.
[[nodiscard]] bool holds_gil_for(const IGame& game, const std::vector<IStrategy*>& strategies);

// Runs the matches in ~50 ms chunks (GIL released unless holds_gil) and raises KeyboardInterrupt between chunks on Ctrl-C.
[[nodiscard]] BatchResult run_interruptible(BatchRunner& runner, int32_t match_count, bool holds_gil);

} // namespace oryx::python
