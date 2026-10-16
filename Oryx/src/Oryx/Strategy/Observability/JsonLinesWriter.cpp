#include "JsonLinesWriter.h"

namespace oryx
{

namespace
{

std::string quoted(const std::string& text)
{
    std::string result = "\"";
    for (char character : text)
    {
        switch (character)
        {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20)
                {
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned>(character));
                    result += escape;
                }
                else
                {
                    result += character;
                }
        }
    }
    return result + "\"";
}

std::string number(double value)
{
    if (!std::isfinite(value))
    {
        return "null";
    }
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return stream.str();
}

void write_action(std::ostream& out, const IState& state, ActionId action, const IActionDescriber* describer, const char* label_key, const char* spaces_key)
{
    ActionDescription description;
    bool described = describer != nullptr && is_game_action(action) && describer->describe(state, action, description);
    std::string label = described ? std::string(label_view(description.label)) : (is_game_action(action) ? state.action_to_string(action) : std::string());
    out << "\"" << label_key << "\":" << quoted(label);
    if (described)
    {
        out << ",\"" << spaces_key << "\":[";
        for (uint8_t i = 0; i < description.space_count; ++i)
        {
            out << (i == 0 ? "" : ",") << std::to_string(description.spaces[i]);
        }
        out << "]";
    }
}

} // namespace

JsonLinesWriter::JsonLinesWriter(std::ostream& out, TraceOptions options)
    : m_out(out)
    , m_options(std::move(options))
    , m_match_started(std::chrono::steady_clock::now())
{
    const TraceHeader& header = m_options.header;
    m_out << "{\"kind\":\"" << trace_kind::header << "\",\"schema_version\":" << std::to_string(k_trace_schema_version) << ",\"game\":" << quoted(header.game) << ",\"params\":{";
    bool first = true;
    for (const auto& [key, value] : header.params)
    {
        m_out << (first ? "" : ",") << quoted(key) << ":";
        if (const bool* flag = std::get_if<bool>(&value))
        {
            m_out << (*flag ? "true" : "false");
        }
        else if (const int64_t* integer = std::get_if<int64_t>(&value))
        {
            m_out << std::to_string(*integer);
        }
        else if (const double* real = std::get_if<double>(&value))
        {
            m_out << number(*real);
        }
        else
        {
            m_out << quoted(std::get<std::string>(value));
        }
        first = false;
    }
    m_out << "},\"players\":[";
    for (size_t i = 0; i < header.players.size(); ++i)
    {
        m_out << (i == 0 ? "" : ",") << quoted(header.players[i]);
    }
    m_out << "],\"seed\":" << std::to_string(header.seed) << ",\"build\":" << quoted(header.build) << "}\n";
}

void JsonLinesWriter::on_match_start()
{
    if (m_ply > 0)
    {
        ++m_match;
    }
    m_ply = 0;
    m_match_started = std::chrono::steady_clock::now();
}

void JsonLinesWriter::on_match_end(const Outcome& outcome)
{
    m_out << "{\"kind\":\"" << trace_kind::match_end << "\",\"match\":" << std::to_string(m_match) << ",\"rewards\":[";
    for (size_t i = 0; i < outcome.rewards.player_count(); ++i)
    {
        m_out << (i == 0 ? "" : ",") << number(outcome.rewards[static_cast<PlayerId>(i)]);
    }
    m_out << "]}\n";
}

void JsonLinesWriter::on_decision(const IState& state, const Decision& decision)
{
    const IActionDescriber* describer = m_options.describer.get();
    m_out << "{\"kind\":\"" << trace_kind::decision << "\",\"match\":" << std::to_string(m_match) << ",\"ply\":" << std::to_string(m_ply++) << ",\"player\":" << std::to_string(decision.player)
          << ",\"chosen\":" << std::to_string(decision.chosen) << ",";
    write_action(m_out, state, decision.chosen, describer, "chosen_label", "chosen_spaces");
    if (m_options.record_time)
    {
        std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_match_started);
        m_out << ",\"t_us\":" << std::to_string(elapsed.count());
    }

    m_out << ",\"scores\":[";
    for (size_t i = 0; i < decision.scores.size(); ++i)
    {
        const ActionScore& score = decision.scores[i];
        m_out << (i == 0 ? "" : ",") << "{\"action\":" << std::to_string(score.action) << ",";
        write_action(m_out, state, score.action, describer, "label", "spaces");
        if (score.has_probability)
        {
            m_out << ",\"probability\":" << number(score.probability);
        }
        if (score.has_value)
        {
            m_out << ",\"value\":" << number(score.value);
        }
        m_out << "}";
    }
    m_out << "],\"extra\":{";
    bool first = true;
    for (const auto& [key, value] : decision.extra.values)
    {
        m_out << (first ? "" : ",") << quoted(key) << ":" << number(value);
        first = false;
    }

    m_out << "},\"tree\":[";
    for (size_t i = 0; i < decision.tree.size(); ++i)
    {
        const SearchNode& node = decision.tree[i];
        m_out << (i == 0 ? "" : ",") << "{\"parent\":" << std::to_string(node.parent) << ",\"action\":" << std::to_string(node.action)
              << ",\"visits\":" << std::to_string(node.visits) << ",\"value\":" << number(node.value) << "}";
    }
    m_out << "]}\n";
}

} // namespace oryx
