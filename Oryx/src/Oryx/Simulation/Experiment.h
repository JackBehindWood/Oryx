#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Core/Params.h"

namespace oryx
{

struct StrategySpec
{
    std::string id;
    Params params;
};

struct Matchup
{
    std::string label;
    std::string game;
    Params game_params;
    std::vector<StrategySpec> seats;
};

struct ExperimentSpec
{
    std::string name;
    std::vector<Matchup> matchups;
    int32_t matches_per_trial = 1;
    int32_t repeats = 1;
    uint64_t master_seed = 0;
};

// `path` is "game.<key>" or "seats.<index>.<key>".
struct SweepAxis
{
    std::string path;
    std::vector<ParamValue> values;
};

class ExperimentError : public Error
{
public:
    using Error::Error;

    [[nodiscard]] const char* category() const noexcept override { return "experiment"; }
};

[[nodiscard]] std::string canonical_string(const Params& params);
[[nodiscard]] std::string strategy_key(const StrategySpec& strategy);
// The label when set, else a canonical string of the matchup's content - never its position in a spec.
[[nodiscard]] std::string matchup_key(const Matchup& matchup);
[[nodiscard]] std::string canonical_string(const ExperimentSpec& spec);
[[nodiscard]] uint64_t spec_hash(const ExperimentSpec& spec);

// Cartesian product over the axes, the last axis varying fastest.
[[nodiscard]] std::vector<Matchup> sweep(const Matchup& base, const std::vector<SweepAxis>& axes);
// Every pair of the pool; with `rotate_seats` both seatings of each pair, cancelling first-player bias.
[[nodiscard]] std::vector<Matchup> round_robin(const std::string& game, const Params& game_params, const std::vector<StrategySpec>& pool, bool rotate_seats);
[[nodiscard]] std::vector<Matchup> self_play(const std::string& game, const Params& game_params, const StrategySpec& strategy, int32_t seats = 2);

// Throws ExperimentError / ParamError naming the matchup, before anything runs.
void validate(const ExperimentSpec& spec);

} // namespace oryx
