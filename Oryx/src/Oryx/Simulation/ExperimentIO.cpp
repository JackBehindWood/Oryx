#include "oxpch.h"
#include "Oryx/Simulation/ExperimentIO.h"

#include "Oryx/Simulation/Statistics.h"

#include <yaml-cpp/yaml.h>

namespace oryx
{

namespace
{

constexpr const char* kCsvHeader = "matchup,repeat,metric,value";

YAML::Node empty_map()
{
    return YAML::Node(YAML::NodeType::Map);
}

YAML::Node params_node(const Params& params)
{
    YAML::Node node = empty_map();
    for (const auto& [key, value] : params)
    {
        node[key]["type"] = to_string(param_type_of(value));
        node[key]["value"] = param_value_text(value);
    }
    return node;
}

Params read_params(const YAML::Node& node)
{
    Params params;
    for (YAML::const_iterator entry = node.begin(); entry != node.end(); ++entry)
    {
        std::string type = entry->second["type"].as<std::string>();
        ParamType param_type = ParamType::Int;
        if (type == "bool") param_type = ParamType::Bool;
        else if (type == "int") param_type = ParamType::Int;
        else if (type == "double") param_type = ParamType::Double;
        else if (type == "string") param_type = ParamType::String;
        else throw ExperimentError("unknown parameter type '" + type + "'");
        params[entry->first.as<std::string>()] = parse_param_value(param_type, entry->second["value"].as<std::string>());
    }
    return params;
}

YAML::Node spec_node(const ExperimentSpec& spec)
{
    YAML::Node node = empty_map();
    node["name"] = spec.name;
    node["matches_per_trial"] = spec.matches_per_trial;
    node["repeats"] = spec.repeats;
    node["master_seed"] = std::to_string(spec.master_seed);
    YAML::Node matchups(YAML::NodeType::Sequence);
    for (const Matchup& matchup : spec.matchups)
    {
        YAML::Node entry = empty_map();
        entry["label"] = matchup.label;
        entry["game"] = matchup.game;
        entry["game_params"] = params_node(matchup.game_params);
        YAML::Node seats(YAML::NodeType::Sequence);
        for (const StrategySpec& seat : matchup.seats)
        {
            YAML::Node seat_node = empty_map();
            seat_node["id"] = seat.id;
            seat_node["params"] = params_node(seat.params);
            seats.push_back(seat_node);
        }
        entry["seats"] = seats;
        matchups.push_back(entry);
    }
    node["matchups"] = matchups;
    return node;
}

ExperimentSpec read_spec(const YAML::Node& node)
{
    ExperimentSpec spec;
    spec.name = node["name"].as<std::string>();
    spec.matches_per_trial = node["matches_per_trial"].as<int32_t>();
    spec.repeats = node["repeats"].as<int32_t>();
    spec.master_seed = std::stoull(node["master_seed"].as<std::string>());
    for (const YAML::Node& entry : node["matchups"])
    {
        Matchup matchup;
        matchup.label = entry["label"].as<std::string>("");
        matchup.game = entry["game"].as<std::string>();
        matchup.game_params = read_params(entry["game_params"]);
        for (const YAML::Node& seat : entry["seats"])
        {
            matchup.seats.push_back({ seat["id"].as<std::string>(), read_params(seat["params"]) });
        }
        spec.matchups.push_back(std::move(matchup));
    }
    return spec;
}

YAML::Node metadata_node(const Metadata& metadata)
{
    YAML::Node node = empty_map();
    node["oryx_version"] = metadata.build.version;
    node["git_hash"] = metadata.build.git_hash;
    node["profile"] = metadata.build.profile;
    node["compiler"] = metadata.build.compiler;
    node["platform"] = metadata.build.platform;
    node["spec_hash"] = std::to_string(metadata.spec_hash);
    node["master_seed"] = std::to_string(metadata.master_seed);
    node["timestamp"] = metadata.timestamp;
    return node;
}

Metadata read_metadata(const YAML::Node& node)
{
    Metadata metadata;
    metadata.build.version = node["oryx_version"].as<std::string>();
    metadata.build.git_hash = node["git_hash"].as<std::string>();
    metadata.build.profile = node["profile"].as<std::string>();
    metadata.build.compiler = node["compiler"].as<std::string>();
    metadata.build.platform = node["platform"].as<std::string>();
    metadata.spec_hash = std::stoull(node["spec_hash"].as<std::string>());
    metadata.master_seed = std::stoull(node["master_seed"].as<std::string>());
    metadata.timestamp = node["timestamp"].as<std::string>("");
    return metadata;
}

YAML::Node summary_node(const ExperimentResult& result)
{
    YAML::Node node(YAML::NodeType::Sequence);
    for (const Matchup& matchup : result.spec.matchups)
    {
        std::string key = matchup_key(matchup);
        YAML::Node entry = empty_map();
        entry["matchup"] = key;
        YAML::Node metrics = empty_map();
        for (const auto& [name, value] : aggregate(result, key).values)
        {
            metrics[name] = format_double(value);
        }
        entry["metrics"] = metrics;
        node.push_back(entry);
    }
    return node;
}

std::string csv_field(const std::string& text)
{
    if (text.find_first_of("\n\r") != std::string::npos)
    {
        throw ExperimentError("'" + text + "' contains a line break and cannot be written to CSV");
    }
    if (text.find_first_of(",\"") == std::string::npos)
    {
        return text;
    }
    std::string quoted = "\"";
    for (char character : text)
    {
        quoted += character == '"' ? "\"\"" : std::string(1, character);
    }
    return quoted + "\"";
}

std::vector<std::string> split_csv_line(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        char character = line[i];
        if (quoted)
        {
            if (character == '"' && i + 1 < line.size() && line[i + 1] == '"')
            {
                field += '"';
                ++i;
            }
            else if (character == '"')
            {
                quoted = false;
            }
            else
            {
                field += character;
            }
        }
        else if (character == '"')
        {
            quoted = true;
        }
        else if (character == ',')
        {
            fields.push_back(std::move(field));
            field.clear();
        }
        else
        {
            field += character;
        }
    }
    fields.push_back(std::move(field));
    return fields;
}

std::vector<TrialResult> read_trials_csv(std::istream& in)
{
    std::string line;
    if (!std::getline(in, line) || line != kCsvHeader)
    {
        throw ExperimentError(std::string(kTrialsFileName) + ": missing or unrecognised header");
    }

    std::vector<TrialResult> trials;
    while (std::getline(in, line))
    {
        if (line.empty())
        {
            continue;
        }
        std::vector<std::string> fields = split_csv_line(line);
        if (fields.size() != 4)
        {
            throw ExperimentError(std::string(kTrialsFileName) + ": malformed row '" + line + "'");
        }
        int32_t repeat = static_cast<int32_t>(std::stol(fields[1]));
        if (trials.empty() || trials.back().matchup != fields[0] || trials.back().repeat != repeat)
        {
            TrialResult trial;
            trial.matchup = fields[0];
            trial.repeat = repeat;
            trials.push_back(std::move(trial));
        }
        trials.back().metrics.values[fields[2]] = std::stod(fields[3]);
    }
    return trials;
}

std::filesystem::path require_file(const std::filesystem::path& directory, const char* name)
{
    std::filesystem::path path = directory / name;
    if (!std::filesystem::is_regular_file(path))
    {
        throw ExperimentError("result directory '" + directory.string() + "' has no " + name);
    }
    return path;
}

} // namespace

void write_trials_csv(const ExperimentResult& result, std::ostream& out)
{
    out << kCsvHeader << '\n';
    for (const TrialResult& trial : result.trials)
    {
        std::string prefix = csv_field(trial.matchup) + "," + std::to_string(trial.repeat) + ",";
        for (const auto& [metric, value] : trial.metrics.values)
        {
            out << prefix << csv_field(metric) << "," << format_double(value) << '\n';
        }
    }
}

void save_result(const ExperimentResult& result, const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);

    YAML::Node root = empty_map();
    root["schema_version"] = kResultSchemaVersion;
    root["spec"] = spec_node(result.spec);
    root["metadata"] = metadata_node(result.metadata);
    root["trial_count"] = static_cast<int64_t>(result.trials.size());
    root["summary"] = summary_node(result);

    std::ofstream yaml(directory / kResultFileName);
    yaml << YAML::Dump(root) << '\n';
    std::ofstream csv(directory / kTrialsFileName);
    write_trials_csv(result, csv);
    if (!yaml || !csv)
    {
        throw ExperimentError("could not write results into '" + directory.string() + "'");
    }
}

ExperimentResult load_result(const std::filesystem::path& directory)
{
    std::filesystem::path yaml_path = require_file(directory, kResultFileName);
    std::filesystem::path csv_path = require_file(directory, kTrialsFileName);

    ExperimentResult result;
    int64_t expected_trials = 0;
    try
    {
        YAML::Node root = YAML::LoadFile(yaml_path.string());
        int32_t version = root["schema_version"].as<int32_t>();
        if (version != kResultSchemaVersion)
        {
            throw ExperimentError("result schema_version " + std::to_string(version) + " is not supported (this build reads " + std::to_string(kResultSchemaVersion) + ")");
        }
        result.spec = read_spec(root["spec"]);
        result.metadata = read_metadata(root["metadata"]);
        expected_trials = root["trial_count"].as<int64_t>();
    }
    catch (const YAML::Exception& error)
    {
        throw ExperimentError("'" + yaml_path.string() + "' is not a valid result file: " + error.what());
    }

    if (spec_hash(result.spec) != result.metadata.spec_hash)
    {
        throw ExperimentError("'" + yaml_path.string() + "': the spec no longer matches its recorded spec_hash");
    }

    std::ifstream csv(csv_path);
    try
    {
        result.trials = read_trials_csv(csv);
    }
    catch (const std::logic_error& error)
    {
        throw ExperimentError("'" + csv_path.string() + "' is not a valid trials file: " + error.what());
    }
    if (static_cast<int64_t>(result.trials.size()) != expected_trials)
    {
        throw ExperimentError("'" + csv_path.string() + "' has " + std::to_string(result.trials.size()) + " trials but the result records " + std::to_string(expected_trials));
    }
    return result;
}

} // namespace oryx
