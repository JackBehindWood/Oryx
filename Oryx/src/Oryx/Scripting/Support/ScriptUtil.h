#pragma once

#include "Oryx/Core/Params.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Game/IState.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Scripting/Support/ScriptError.h"
#include "Oryx/Scripting/Support/ScriptOrigin.h"
#include "Oryx/Scripting/Support/ScriptSource.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

[[nodiscard]] std::string describe(const ScriptOrigin& origin);
[[nodiscard]] std::string describe(const ScriptSource& source);

// "<context>: <message>" with the runtime's traceback as detail.
[[nodiscard]] ScriptError script_error(std::string_view context, std::string_view message, std::string traceback = "");

// A script may only play actions its state lists as legal (D27); `context`, when given, prefixes the message.
void check_legal(const IState& state, ActionId action, std::string_view context = {});

// Rewards a script reported for one state; throws ScriptError unless there is exactly one per player.
[[nodiscard]] Outcome outcome_from_rewards(std::span<const double> rewards, size_t player_count, bool terminal);
[[nodiscard]] std::vector<double> rewards_to_vector(const Rewards<double>& rewards);

// True when any participant is backed by a script, so a runtime that holds a global lock must keep it.
[[nodiscard]] bool involves_script(const IGame& game, std::span<IStrategy* const> strategies);

// A strategy created by name that declares a `seed` parameter gets master_seed + seat (D17); anything else gets none.
[[nodiscard]] Params seeded_params(const std::string& strategy, int64_t master_seed, size_t seat);

} // namespace oryx
