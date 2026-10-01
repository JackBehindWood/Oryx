#include "oxpch.h"
#include "Oryx/Simulation/Experiment.h"

#include "Oryx/Game/IGame.h"
#include "Oryx/Simulation/Match.h"
#include "Oryx/Simulation/SeedSequence.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

namespace
{

std::string typed_param_value_text(const ParamValue& value)
{
    switch (param_type_of(value))
    {
        case ParamType::Bool: return "b:" + param_value_text(value);
        case ParamType::Int: return "i:" + param_value_text(value);
        case ParamType::Double: return "d:" + param_value_text(value);
        case ParamType::String: return "s:\"" + param_value_text(value) + "\"";
    }
    return "";
}

Params& params_at_path(Matchup& matchup, const std::string& path, std::string& key)
{
    constexpr std::string_view kGame = "game.";
    constexpr std::string_view kSeats = "seats.";
    if (path.compare(0, kGame.size(), kGame) == 0 && path.size() > kGame.size())
    {
        key = path.substr(kGame.size());
        return matchup.game_params;
    }
    if (path.compare(0, kSeats.size(), kSeats) == 0)
    {
        size_t dot = path.find('.', kSeats.size());
        if (dot != std::string::npos && dot > kSeats.size() && dot + 1 < path.size())
        {
            std::string index_text = path.substr(kSeats.size(), dot - kSeats.size());
            if (index_text.find_first_not_of("0123456789") == std::string::npos)
            {
                size_t index = static_cast<size_t>(std::stoul(index_text));
                if (index < matchup.seats.size())
                {
                    key = path.substr(dot + 1);
                    return matchup.seats[index].params;
                }
                throw ExperimentError("sweep path '" + path + "': the matchup has only " + std::to_string(matchup.seats.size()) + " seats");
            }
        }
    }
    throw ExperimentError("sweep path '" + path + "' must be 'game.<key>' or 'seats.<index>.<key>'");
}

Matchup make_matchup(const std::string& game, const Params& game_params, std::vector<StrategySpec> seats)
{
    Matchup matchup;
    matchup.game = game;
    matchup.game_params = game_params;
    matchup.seats = std::move(seats);
    std::string label;
    for (const StrategySpec& seat : matchup.seats)
    {
        label += label.empty() ? strategy_key(seat) : " vs " + strategy_key(seat);
    }
    matchup.label = std::move(label);
    return matchup;
}

} // namespace

std::string format_double(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

std::string param_value_text(const ParamValue& value)
{
    switch (param_type_of(value))
    {
        case ParamType::Bool: return std::get<bool>(value) ? "true" : "false";
        case ParamType::Int: return std::to_string(std::get<int64_t>(value));
        case ParamType::Double: return format_double(std::get<double>(value));
        case ParamType::String: return std::get<std::string>(value);
    }
    return "";
}

ParamValue parse_param_value(ParamType type, const std::string& text)
{
    try
    {
        switch (type)
        {
            case ParamType::Bool:
                if (text != "true" && text != "false")
                {
                    throw ExperimentError("'" + text + "' is not a bool");
                }
                return text == "true";
            case ParamType::Int: return static_cast<int64_t>(std::stoll(text));
            case ParamType::Double: return std::stod(text);
            case ParamType::String: return text;
        }
    }
    catch (const std::logic_error&)
    {
        throw ExperimentError("'" + text + "' is not a valid " + to_string(type));
    }
    throw ExperimentError("unknown parameter type");
}

std::string canonical_string(const Params& params)
{
    std::string text;
    for (const auto& [key, value] : params)
    {
        text += (text.empty() ? "" : ",") + key + "=" + typed_param_value_text(value);
    }
    return text;
}

std::string strategy_key(const StrategySpec& strategy)
{
    return strategy.params.empty() ? strategy.id : strategy.id + "(" + canonical_string(strategy.params) + ")";
}

std::string matchup_key(const Matchup& matchup)
{
    if (!matchup.label.empty())
    {
        return matchup.label;
    }
    std::string key = matchup.game + "(" + canonical_string(matchup.game_params) + ")";
    for (const StrategySpec& seat : matchup.seats)
    {
        key += "|" + strategy_key(seat);
    }
    return key;
}

std::string canonical_string(const ExperimentSpec& spec)
{
    std::string text = "name=" + spec.name + ";matches=" + std::to_string(spec.matches_per_trial) + ";repeats=" + std::to_string(spec.repeats)
        + ";seed=" + std::to_string(spec.master_seed);
    for (const Matchup& matchup : spec.matchups)
    {
        text += ";[" + matchup_key(matchup) + "|" + matchup.game + "(" + canonical_string(matchup.game_params) + ")";
        for (const StrategySpec& seat : matchup.seats)
        {
            text += "|" + strategy_key(seat);
        }
        text += "]";
    }
    return text;
}

uint64_t spec_hash(const ExperimentSpec& spec)
{
    return hash_string(canonical_string(spec));
}

std::vector<Matchup> sweep(const Matchup& base, const std::vector<SweepAxis>& axes)
{
    for (const SweepAxis& axis : axes)
    {
        if (axis.values.empty())
        {
            throw ExperimentError("sweep path '" + axis.path + "' has no values");
        }
    }

    std::string base_key = matchup_key(base);
    std::vector<Matchup> matchups;
    std::vector<size_t> cursor(axes.size(), 0);
    while (true)
    {
        Matchup matchup = base;
        std::string suffix;
        for (size_t axis = 0; axis < axes.size(); ++axis)
        {
            std::string key;
            const ParamValue& value = axes[axis].values[cursor[axis]];
            params_at_path(matchup, axes[axis].path, key)[key] = value;
            suffix += (suffix.empty() ? "" : ",") + axes[axis].path + "=" + param_value_text(value);
        }
        if (!axes.empty())
        {
            matchup.label = base_key + " [" + suffix + "]";
        }
        matchups.push_back(std::move(matchup));

        size_t axis = axes.size();
        while (axis > 0 && ++cursor[axis - 1] == axes[axis - 1].values.size())
        {
            cursor[--axis] = 0;
        }
        if (axis == 0)
        {
            return matchups;
        }
    }
}

std::vector<Matchup> round_robin(const std::string& game, const Params& game_params, const std::vector<StrategySpec>& pool, bool rotate_seats)
{
    std::vector<Matchup> matchups;
    for (size_t first = 0; first < pool.size(); ++first)
    {
        for (size_t second = first + 1; second < pool.size(); ++second)
        {
            matchups.push_back(make_matchup(game, game_params, { pool[first], pool[second] }));
            if (rotate_seats && strategy_key(pool[first]) != strategy_key(pool[second]))
            {
                matchups.push_back(make_matchup(game, game_params, { pool[second], pool[first] }));
            }
        }
    }
    return matchups;
}

std::vector<Matchup> self_play(const std::string& game, const Params& game_params, const StrategySpec& strategy, int32_t seats)
{
    if (seats < 1)
    {
        throw ExperimentError("self_play needs at least one seat");
    }
    return { make_matchup(game, game_params, std::vector<StrategySpec>(static_cast<size_t>(seats), strategy)) };
}

void validate(const ExperimentSpec& spec)
{
    if (spec.matchups.empty())
    {
        throw ExperimentError("experiment '" + spec.name + "' has no matchups");
    }
    if (spec.matches_per_trial < 1 || spec.repeats < 1)
    {
        throw ExperimentError("experiment '" + spec.name + "': matches_per_trial and repeats must be at least 1");
    }

    std::set<std::string> keys;
    for (const Matchup& matchup : spec.matchups)
    {
        std::string key = matchup_key(matchup);
        std::string where = "matchup '" + key + "': ";
        if (!keys.insert(key).second)
        {
            throw ExperimentError(where + "duplicate matchup key; give one of them a distinct label");
        }

        try
        {
            UniquePtr<IGame> game = create_game(matchup.game, matchup.game_params);
            if (game == nullptr)
            {
                throw ExperimentError(where + "unknown game '" + matchup.game + "'");
            }
            if (static_cast<size_t>(game->num_players()) != matchup.seats.size())
            {
                throw ExperimentError(where + "game '" + matchup.game + "' has " + std::to_string(game->num_players()) + " players but "
                    + std::to_string(matchup.seats.size()) + " seats were given");
            }

            UniquePtr<IState> state = game->new_initial_state();
            Context context = Match::build_context(*game, *state);
            for (size_t seat = 0; seat < matchup.seats.size(); ++seat)
            {
                const StrategySpec& strategy_spec = matchup.seats[seat];
                UniquePtr<IStrategy> strategy = StrategyRegistry::create(strategy_spec.id, strategy_spec.params);
                if (strategy == nullptr)
                {
                    throw ExperimentError(where + "unknown strategy '" + strategy_spec.id + "' in seat " + std::to_string(seat));
                }
                if (!Match::missing_capabilities(*strategy, context).empty())
                {
                    throw ExperimentError(where + "strategy '" + strategy_spec.id + "' in seat " + std::to_string(seat)
                        + " requires a capability game '" + matchup.game + "' does not provide");
                }
            }
        }
        catch (const ParamError& error)
        {
            throw ExperimentError(where + error.what());
        }
    }
}

} // namespace oryx
