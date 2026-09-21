#pragma once

#include "Oryx/Game/IGame.h"
#include "Oryx/Scripting/ScriptError.h"
#include "Oryx/Scripting/ScriptOrigin.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

// Registers script-defined entries into GameRegistry/StrategyRegistry, remembering where each came from.
// Re-registering under the same origin (a re-run cell, a reloaded file) replaces the entry; a clash with a C++ entry or
// another origin throws ScriptError unless overwrite is set.
void register_scripted_game(const std::string& id, const ScriptOrigin& origin, GameRegistry::Factory factory, EntryInfo info, bool overwrite = false);
void register_scripted_strategy(const std::string& id, const ScriptOrigin& origin, StrategyRegistry::Factory factory, EntryInfo info, bool overwrite = false);

// Drops every game and strategy registered by a language's scripts; runtimes call it before shutting down.
void unregister_scripted(const std::string& language);

// nullptr when the entry is unregistered or implemented in C++.
[[nodiscard]] const ScriptOrigin* scripted_game_origin(const std::string& id);
[[nodiscard]] const ScriptOrigin* scripted_strategy_origin(const std::string& id);

} // namespace oryx
