#pragma once

#include "Oryx/Game/IGame.h"
#include "Oryx/Scripting/Support/ScriptError.h"
#include "Oryx/Scripting/Support/ScriptOrigin.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

// Registers script entries into GameRegistry/StrategyRegistry: the same origin replaces its entry, any other clash throws ScriptError unless overwrite is set.
void register_scripted_game(const std::string& id, const ScriptOrigin& origin, GameRegistry::Factory factory, EntryInfo info, bool overwrite = false);
void register_scripted_strategy(const std::string& id, const ScriptOrigin& origin, StrategyRegistry::Factory factory, EntryInfo info, bool overwrite = false);

// Drops every game and strategy layer of a language's scripts and restores the topmost remaining entry each one overwrote; runtimes call it before shutting down.
void unregister_scripted(const std::string& language);

// nullptr when the entry is unregistered or implemented in C++.
[[nodiscard]] const ScriptOrigin* scripted_game_origin(const std::string& id);
[[nodiscard]] const ScriptOrigin* scripted_strategy_origin(const std::string& id);

} // namespace oryx
