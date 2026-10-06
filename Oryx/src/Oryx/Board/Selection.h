#pragma once

#include "Oryx/Core/CommandLine.h"
#include "Oryx/Core/Registry.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Strategy/IStrategy.h"

// Picks games and opponents by name for a front-end, from the registries.
namespace oryx::selection
{

constexpr const char* k_human_opponent = "human";

// What --game and --opponent asked for; empty means "pick the default (the terminal asks when it can)".
struct BoardOptions
{
    std::string game;
    std::string opponent;
};

[[nodiscard]] BoardOptions board_options(const ParsedArgs& args);

enum class FrontEnd
{
    Console,
    Graphical
};

[[nodiscard]] std::vector<std::string> sorted(std::vector<std::string> names);
[[nodiscard]] std::string joined(const std::vector<std::string>& names);
[[nodiscard]] bool needs_params(const EntryInfo* info);
// Oasis has no way to supply parameters yet, so an entry with required ones cannot be created; throws Error.
void require_creatable(const char* what, const std::string& name, const EntryInfo* info);
[[nodiscard]] std::vector<std::string> strategies_for(const std::string& game);

// A non-empty request must name a creatable entry. Otherwise the choice is prompted on stdin when `prompt` is set, else defaulted. False means exit.
[[nodiscard]] bool choose_game(const std::string& requested, bool prompt, std::string& out_name);
// Candidates are the game's strategies plus k_human_opponent.
[[nodiscard]] bool choose_opponent(const std::string& game, const std::string& requested, bool prompt, std::string& out_name);

// Graphical only when graphics are built and wanted and the game has a graphics board or a presenter; otherwise Console.
// A graphical choice resolves the game now (defaulting it, never prompting); a console choice keeps `requested`, which BoardLayer may still prompt for. False means exit.
[[nodiscard]] bool choose_front_end(const std::string& requested_game, bool headless, bool graphics_built, FrontEnd& out_front_end, std::string& out_game);

[[nodiscard]] UniquePtr<IStrategy> create_opponent(const std::string& name, bool announce);

} // namespace oryx::selection
