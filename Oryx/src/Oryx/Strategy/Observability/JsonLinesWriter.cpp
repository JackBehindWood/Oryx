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

} // namespace

JsonLinesWriter::JsonLinesWriter(std::ostream& out)
    : m_out(out)
{
    m_out << "{\"schema_version\":" << std::to_string(k_trace_schema_version) << "}\n";
}

void JsonLinesWriter::on_decision(const IState& state, const Decision& decision)
{
    m_out << "{\"ply\":" << std::to_string(m_ply++) << ",\"player\":" << std::to_string(decision.player) << ",\"chosen\":" << std::to_string(decision.chosen)
          << ",\"chosen_label\":" << quoted(is_game_action(decision.chosen) ? state.action_to_string(decision.chosen) : std::string());

    m_out << ",\"scores\":[";
    for (size_t i = 0; i < decision.scores.size(); ++i)
    {
        const ActionScore& score = decision.scores[i];
        m_out << (i == 0 ? "" : ",") << "{\"action\":" << std::to_string(score.action) << ",\"label\":" << quoted(state.action_to_string(score.action));
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
