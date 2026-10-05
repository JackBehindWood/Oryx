#include "Selection.h"

#include "Oryx/Board/ConsoleGame.h"
#include "Oryx/Board/IBoardPresenter.h"
#include "Oryx/Board/IGraphicsBoard.h"
#include "Oryx/Core/Log.h"
#include <iostream>

namespace oryx::selection
{

using InfoLookup = std::function<const EntryInfo*(const std::string&)>;

namespace
{

constexpr const char* kDefaultGame = "tictactoe";
constexpr const char* kDefaultOpponent = "minimax";
constexpr const char* kGameOption = "game";
constexpr const char* kOpponentOption = "opponent";

class BoardCommandLine : public ICommandLineContributor
{
public:
    void declare(CommandLine& command_line) const override
    {
        command_line.option(kGameOption, "NAME", "Game to play (default: tictactoe)")
            .option(kOpponentOption, "NAME", "Opponent strategy, or 'human' for hot-seat");
    }
};

OX_REGISTER_COMMAND_LINE(BoardCommandLine, "board")

// A strategy named "<game>/<name>" only understands that game's states; unprefixed strategies play anything.
bool fits_game(const std::string& strategy, const std::string& game)
{
    size_t slash = strategy.find('/');
    return slash == std::string::npos || strategy.substr(0, slash) == game;
}

std::string display(const ParamValue& value)
{
    return std::visit([](const auto& held) -> std::string
    {
        using Held = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<Held, bool>)
        {
            return held ? "true" : "false";
        }
        else if constexpr (std::is_same_v<Held, std::string>)
        {
            return held;
        }
        else
        {
            std::ostringstream stream;
            stream << held;
            return stream.str();
        }
    }, value);
}

std::string describe_param(const ParamSpec& spec)
{
    if (spec.has_default)
    {
        return spec.name + "=" + display(spec.default_value);
    }
    return spec.name + (spec.required ? " (required)" : " (no default)");
}

std::string describe_entry(const std::string& name, const EntryInfo* info)
{
    std::string line = "  " + name;
    if (info == nullptr)
    {
        return line;
    }

    if (!info->description.empty())
    {
        line += " - " + info->description;
    }

    std::string params;
    for (const ParamSpec& spec : info->schema)
    {
        params += (params.empty() ? "" : ", ") + describe_param(spec);
    }
    return params.empty() ? line : line + " [" + params + "]";
}

bool prompt_for_choice(const std::string& what, const std::vector<std::string>& names, const InfoLookup& info, std::string& out_name)
{
    for (const std::string& name : names)
    {
        std::cout << describe_entry(name, info(name)) << "\n";
    }

    while (true)
    {
        std::cout << "Choose " << what << " - " << joined(names) << ": ";

        std::string line;
        if (!std::getline(std::cin, line))
        {
            return false;
        }

        if (std::find(names.begin(), names.end(), line) != names.end())
        {
            out_name = line;
            return true;
        }

        std::cout << "Unrecognized choice. Try again.\n";
    }
}

} // namespace

BoardOptions board_options(const ParsedArgs& args)
{
    return { args.value(kGameOption), args.value(kOpponentOption) };
}

std::vector<std::string> sorted(std::vector<std::string> names)
{
    std::sort(names.begin(), names.end());
    return names;
}

std::string joined(const std::vector<std::string>& names)
{
    std::string result;
    for (const std::string& name : names)
    {
        result += (result.empty() ? "" : ", ") + name;
    }
    return result;
}

bool needs_params(const EntryInfo* info)
{
    return info != nullptr && !required_param_names(info->schema).empty();
}

void require_creatable(const char* what, const std::string& name, const EntryInfo* info)
{
    if (needs_params(info))
    {
        throw Error(std::string(what) + " '" + name + "' has required parameters (" + joined(required_param_names(info->schema)) + ") that Oasis cannot supply yet");
    }
}

std::vector<std::string> strategies_for(const std::string& game)
{
    std::vector<std::string> names;
    for (const std::string& name : StrategyRegistry::names())
    {
        if (fits_game(name, game) && !needs_params(StrategyRegistry::info(name)))
        {
            names.push_back(name);
        }
    }
    return sorted(std::move(names));
}

bool choose_game(const std::string& requested, bool prompt, std::string& out_name)
{
    std::vector<std::string> names;
    for (const std::string& name : sorted(GameRegistry::names()))
    {
        if (!needs_params(GameRegistry::info(name)))
        {
            names.push_back(name);
        }
    }

    if (!requested.empty())
    {
        if (!GameRegistry::has(requested))
        {
            OX_ERROR("Unknown --game '{}' - registered games: {}.", requested, joined(names));
            return false;
        }
        require_creatable("game", requested, GameRegistry::info(requested));
        out_name = requested;
        return true;
    }

    if (names.empty())
    {
        OX_ERROR("No game without required parameters is registered.");
        return false;
    }

    if (names.size() == 1 || !prompt)
    {
        out_name = GameRegistry::has(kDefaultGame) ? kDefaultGame : names.front();
        if (names.size() > 1)
        {
            OX_INFO("Playing '{}' (default). Games: {}. Pick one with --game=NAME.", out_name, joined(names));
        }
        return true;
    }

    if (!prompt_for_choice("a game", names, [](const std::string& name) { return GameRegistry::info(name); }, out_name))
    {
        OX_INFO("Input closed before a game was chosen - exiting.");
        return false;
    }
    return true;
}

bool choose_opponent(const std::string& game, const std::string& requested, bool prompt, std::string& out_name)
{
    std::vector<std::string> opponents = strategies_for(game);
    opponents.insert(opponents.begin(), kHumanOpponent);

    if (!requested.empty())
    {
        require_creatable("opponent", requested, StrategyRegistry::info(requested));
        if (std::find(opponents.begin(), opponents.end(), requested) == opponents.end())
        {
            OX_ERROR("Unknown --opponent '{}' for '{}' - must be one of: {}.", requested, game, joined(opponents));
            return false;
        }
        out_name = requested;
        return true;
    }

    if (!prompt)
    {
        bool has_default = std::find(opponents.begin(), opponents.end(), kDefaultOpponent) != opponents.end();
        out_name = has_default ? kDefaultOpponent : opponents.back();
        return true;
    }

    if (!prompt_for_choice("an opponent", opponents, [](const std::string& name) { return StrategyRegistry::info(name); }, out_name))
    {
        OX_INFO("Input closed before an opponent was chosen - exiting.");
        return false;
    }
    return true;
}

bool choose_front_end(const std::string& requested_game, bool headless, bool graphics_built, FrontEnd& out_front_end, std::string& out_game)
{
    out_front_end = FrontEnd::Console;
    out_game = requested_game;
    if (headless || !graphics_built)
    {
        return true;
    }

    std::string game;
    if (!choose_game(requested_game, false, game))
    {
        return false;
    }

    out_game = game;
    if (GraphicsBoardRegistry::has(game) || BoardPresenterRegistry::has(game))
    {
        out_front_end = FrontEnd::Graphical;
    }
    else
    {
        OX_INFO("No graphics board or presenter for '{}' - playing in the terminal.", game);
    }
    return true;
}

UniquePtr<IStrategy> create_opponent(const std::string& name, bool announce)
{
    UniquePtr<IStrategy> opponent = StrategyRegistry::create(name);
    if (announce)
    {
        return create_unique<AnnouncingStrategy>(std::move(opponent), name);
    }
    return opponent;
}

} // namespace oryx::selection
