#include "oxpch.h"
#include "Oryx/Scripting/Support/ScriptUtil.h"

#include "Oryx/Scripting/Interfaces/IScriptedGame.h"
#include "Oryx/Scripting/Interfaces/IScriptedStrategy.h"

namespace oryx
{

std::string describe(const ScriptOrigin& origin)
{
    std::string result = origin.language + " module '" + origin.module + "'";
    return origin.source_file.empty() ? result : result + " (" + origin.source_file + ")";
}

std::string describe(const ScriptSource& source)
{
    return source.kind == ScriptSourceKind::Module ? "module '" + source.target + "'" : "script '" + source.target + "'";
}

ScriptError script_error(std::string_view context, std::string_view message, std::string traceback)
{
    return ScriptError(std::string(context) + ": " + std::string(message), std::move(traceback));
}

void check_legal(const IState& state, ActionId action, std::string_view context)
{
    ActionList legal = state.legal_actions();
    if (std::find(legal.begin(), legal.end(), action) != legal.end())
    {
        return;
    }

    std::string message = "action " + to_string(action) + " is not legal in this state";
    throw ScriptError(context.empty() ? message : std::string(context) + ": " + message);
}

Outcome outcome_from_rewards(std::span<const double> rewards, size_t player_count, bool terminal)
{
    if (rewards.size() != player_count)
    {
        throw ScriptError("state.outcome(): expected " + std::to_string(player_count) + " rewards but got " + std::to_string(rewards.size()));
    }

    Outcome result;
    result.is_terminal = terminal;
    result.rewards = Rewards<double>(player_count);
    for (size_t player = 0; player < player_count; ++player)
    {
        result.rewards[static_cast<PlayerId>(player)] = rewards[player];
    }
    return result;
}

std::vector<double> rewards_to_vector(const Rewards<double>& rewards)
{
    std::vector<double> result;
    result.reserve(rewards.player_count());
    for (size_t player = 0; player < rewards.player_count(); ++player)
    {
        result.push_back(rewards[static_cast<PlayerId>(player)]);
    }
    return result;
}

bool involves_script(const IGame& game, std::span<IStrategy* const> strategies)
{
    if (dynamic_cast<const IScriptedGame*>(&game) != nullptr)
    {
        return true;
    }
    return std::any_of(strategies.begin(), strategies.end(), [](const IStrategy* strategy) { return dynamic_cast<const IScriptedStrategy*>(strategy) != nullptr; });
}

Params seeded_params(const std::string& strategy, int64_t master_seed, size_t seat)
{
    const EntryInfo* info = StrategyRegistry::info(strategy);
    if (info == nullptr || std::none_of(info->schema.begin(), info->schema.end(), [](const ParamSpec& spec) { return spec.name == "seed"; }))
    {
        return {};
    }
    return Params{ { "seed", ParamValue(master_seed + static_cast<int64_t>(seat)) } };
}

} // namespace oryx
