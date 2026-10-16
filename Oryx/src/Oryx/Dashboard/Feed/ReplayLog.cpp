#include "ReplayLog.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Metrics.h"
#include "Oryx/Strategy/Observability/TraceSchema.h"

namespace oryx
{

namespace
{

constexpr size_t k_decode_capacity = 256;
constexpr size_t k_read_chunk = 2048;
constexpr size_t k_index_block = 64 * 1024;
// A metric needs at least `"k":1,` so this many fit in one line.
constexpr size_t k_min_metric_bytes = 6;

enum class LineKind : uint8_t
{
    Bad,
    Unknown,
    Header,
    Decision
};

struct ScanInfo
{
    int64_t match = 0;
    int64_t schema_version = 0;
    bool has_time = false;
};

struct Text
{
    std::string_view view;
    bool truncated = false;
};

enum class Next : uint8_t
{
    Item,
    End,
    Error
};

template<typename T>
bool convert(std::string_view token, T& value)
{
    const char* end = token.data() + token.size();
    std::from_chars_result result = std::from_chars(token.data(), end, value);
    return !token.empty() && result.ec == std::errc() && result.ptr == end;
}

bool convert(std::string_view token, double& value)
{
#if defined(__cpp_lib_to_chars)
    const char* end = token.data() + token.size();
    std::from_chars_result result = std::from_chars(token.data(), end, value);
    return !token.empty() && result.ec == std::errc() && result.ptr == end;
#else
    // The line buffer is NUL-terminated, so strtod stops at the token's end.
    char* parsed_end = nullptr;
    value = std::strtod(token.data(), &parsed_end);
    return !token.empty() && parsed_end == token.data() + token.size() && std::isfinite(value);
#endif
}

// One zero-allocation pass over a single line; never throws, never recurses on input depth.
class Scanner
{
public:
    Scanner(const char* begin, const char* end, uint32_t max_depth)
        : m_p(begin)
        , m_end(end)
        , m_max_depth(max_depth)
    {
    }

    LineKind scan(ParsedRecord& out, ReplayKeyTable& keys, ReplayHeader* header, ScanInfo& info)
    {
        out.record = DashboardRecord{};
        m_out = &out;
        m_keys = &keys;
        if (!open_container('{'))
        {
            return LineKind::Bad;
        }
        LineKind kind = LineKind::Unknown;
        bool first = true;
        Text key;
        for (;;)
        {
            Next next = next_item(first, '}');
            first = false;
            if (next == Next::End)
            {
                break;
            }
            if (next == Next::Error || !parse_string(key) || !consume(':'))
            {
                return LineKind::Bad;
            }
            if (!member(key.view, kind, header, info))
            {
                return LineKind::Bad;
            }
        }
        skip_ws();
        if (m_p != m_end)
        {
            return LineKind::Bad;
        }
        if (kind == LineKind::Decision)
        {
            finish_chosen();
        }
        return kind;
    }

private:
    bool member(std::string_view key, LineKind& kind, ReplayHeader* header, ScanInfo& info)
    {
        DashboardRecord& record = m_out->record;
        if (key == "kind")
        {
            Text text;
            if (!parse_string(text))
            {
                return false;
            }
            kind = text.view == trace_kind::decision ? LineKind::Decision : (text.view == trace_kind::header ? LineKind::Header : LineKind::Unknown);
            return true;
        }
        if (key == "schema_version")
        {
            return parse_int(info.schema_version);
        }
        if (key == "match")
        {
            bool ok = parse_int(info.match);
            record.match_index = clamp_u32(info.match);
            return ok;
        }
        if (key == "ply")
        {
            int64_t ply = 0;
            record.decision_index = parse_int(ply) ? clamp_u32(ply) : 0;
            return ply >= 0;
        }
        if (key == "player")
        {
            return parse_int(record.player);
        }
        if (key == "chosen")
        {
            m_chosen_seen = parse_int(record.chosen);
            return m_chosen_seen;
        }
        if (key == "chosen_label")
        {
            Text text;
            if (!parse_string(text))
            {
                return false;
            }
            set_label(record.chosen_label, text.view);
            return true;
        }
        if (key == "chosen_spaces")
        {
            uint32_t count = 0;
            if (!parse_spaces(record.chosen_spaces, count))
            {
                return false;
            }
            record.chosen_space_count = static_cast<uint8_t>(count);
            return true;
        }
        if (key == "t_us")
        {
            info.has_time = parse_int(record.time_us);
            return info.has_time;
        }
        if (key == "scores")
        {
            return parse_scores();
        }
        if (key == "extra")
        {
            return parse_extra();
        }
        if (key == "tree")
        {
            return parse_tree();
        }
        if (header != nullptr)
        {
            if (key == "game")
            {
                return parse_owned(header->game);
            }
            if (key == "build")
            {
                return parse_owned(header->build);
            }
            if (key == "seed")
            {
                return parse_int(header->seed);
            }
            if (key == "players")
            {
                return parse_players(header->players);
            }
            if (key == "params")
            {
                return parse_params(header->params);
            }
        }
        return skip_value();
    }

    static uint32_t clamp_u32(int64_t value) { return value < 0 ? 0 : (value > std::numeric_limits<uint32_t>::max() ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(value)); }

    void skip_ws()
    {
        while (m_p < m_end && (*m_p == ' ' || *m_p == '\t' || *m_p == '\r' || *m_p == '\n'))
        {
            ++m_p;
        }
    }

    bool consume(char c)
    {
        skip_ws();
        if (m_p < m_end && *m_p == c)
        {
            ++m_p;
            return true;
        }
        return false;
    }

    bool open_container(char open)
    {
        return consume(open) && ++m_depth <= m_max_depth;
    }

    Next next_item(bool first, char close)
    {
        skip_ws();
        if (m_p >= m_end)
        {
            return Next::Error;
        }
        if (*m_p == close)
        {
            ++m_p;
            --m_depth;
            return Next::End;
        }
        if (!first)
        {
            if (*m_p != ',')
            {
                return Next::Error;
            }
            ++m_p;
        }
        return Next::Item;
    }

    void append(const char* data, size_t count)
    {
        if (m_decoded + count > k_decode_capacity)
        {
            m_decode_truncated = true;
            return;
        }
        std::memcpy(m_decode + m_decoded, data, count);
        m_decoded += count;
    }

    bool hex4(uint32_t& value)
    {
        if (m_end - m_p < 4)
        {
            return false;
        }
        value = 0;
        for (int32_t i = 0; i < 4; ++i)
        {
            char c = *m_p++;
            uint32_t digit = 0;
            if (c >= '0' && c <= '9')
            {
                digit = static_cast<uint32_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = static_cast<uint32_t>(c - 'a' + 10);
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = static_cast<uint32_t>(c - 'A' + 10);
            }
            else
            {
                return false;
            }
            value = value * 16 + digit;
        }
        return true;
    }

    void append_code_point(uint32_t cp)
    {
        char bytes[4];
        size_t count = 0;
        if (cp < 0x80)
        {
            bytes[count++] = static_cast<char>(cp);
        }
        else if (cp < 0x800)
        {
            bytes[count++] = static_cast<char>(0xC0 | (cp >> 6));
            bytes[count++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            bytes[count++] = static_cast<char>(0xE0 | (cp >> 12));
            bytes[count++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            bytes[count++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        else
        {
            bytes[count++] = static_cast<char>(0xF0 | (cp >> 18));
            bytes[count++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            bytes[count++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            bytes[count++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        append(bytes, count);
    }

    bool parse_escape()
    {
        if (m_p >= m_end)
        {
            return false;
        }
        char e = *m_p++;
        switch (e)
        {
            case '"': append("\"", 1); return true;
            case '\\': append("\\", 1); return true;
            case '/': append("/", 1); return true;
            case 'b': append("\b", 1); return true;
            case 'f': append("\f", 1); return true;
            case 'n': append("\n", 1); return true;
            case 'r': append("\r", 1); return true;
            case 't': append("\t", 1); return true;
            case 'u': break;
            default: return false;
        }
        uint32_t cp = 0;
        if (!hex4(cp))
        {
            return false;
        }
        if (cp >= 0xD800 && cp <= 0xDBFF)
        {
            uint32_t low = 0;
            if (m_end - m_p >= 6 && m_p[0] == '\\' && m_p[1] == 'u')
            {
                const char* saved = m_p;
                m_p += 2;
                if (hex4(low) && low >= 0xDC00 && low <= 0xDFFF)
                {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                else
                {
                    m_p = saved;
                    cp = 0xFFFD;
                }
            }
            else
            {
                cp = 0xFFFD;
            }
        }
        else if (cp >= 0xDC00 && cp <= 0xDFFF)
        {
            cp = 0xFFFD;
        }
        append_code_point(cp);
        return true;
    }

    // Zero-copy unless the text has escapes, then decoded into a fixed buffer that the next string overwrites.
    bool parse_string(Text& out)
    {
        skip_ws();
        if (m_p >= m_end || *m_p != '"')
        {
            return false;
        }
        ++m_p;
        const char* start = m_p;
        while (m_p < m_end && *m_p != '"' && *m_p != '\\')
        {
            ++m_p;
        }
        if (m_p >= m_end)
        {
            return false;
        }
        if (*m_p == '"')
        {
            out.view = std::string_view(start, static_cast<size_t>(m_p - start));
            out.truncated = false;
            ++m_p;
            return true;
        }
        m_decoded = 0;
        m_decode_truncated = false;
        append(start, static_cast<size_t>(m_p - start));
        while (m_p < m_end)
        {
            char c = *m_p;
            if (c == '"')
            {
                ++m_p;
                out.view = std::string_view(m_decode, m_decoded);
                out.truncated = m_decode_truncated;
                return true;
            }
            if (c == '\\')
            {
                ++m_p;
                if (!parse_escape())
                {
                    return false;
                }
                continue;
            }
            const char* run = m_p;
            while (m_p < m_end && *m_p != '"' && *m_p != '\\')
            {
                ++m_p;
            }
            append(run, static_cast<size_t>(m_p - run));
        }
        return false;
    }

    bool parse_owned(std::string& target)
    {
        Text text;
        if (!parse_string(text))
        {
            return false;
        }
        target.assign(text.view);
        return true;
    }

    bool skip_string()
    {
        ++m_p;
        while (m_p < m_end)
        {
            char c = *m_p++;
            if (c == '\\')
            {
                if (m_p >= m_end)
                {
                    return false;
                }
                ++m_p;
            }
            else if (c == '"')
            {
                return true;
            }
        }
        return false;
    }

    // Iterative with a bitmask of open container kinds, so hostile nesting costs a counter, not stack.
    bool skip_value()
    {
        uint64_t objects = 0;
        uint32_t base = m_depth;
        uint32_t depth = base;
        do
        {
            skip_ws();
            if (m_p >= m_end)
            {
                return false;
            }
            char c = *m_p;
            if (c == '"')
            {
                if (!skip_string())
                {
                    return false;
                }
            }
            else if (c == '{' || c == '[')
            {
                if (++depth > m_max_depth)
                {
                    return false;
                }
                uint64_t bit = uint64_t{ 1 } << (depth - 1);
                objects = c == '{' ? (objects | bit) : (objects & ~bit);
                ++m_p;
            }
            else if (c == '}' || c == ']')
            {
                if (depth == base || (((objects >> (depth - 1)) & 1) != 0) != (c == '}'))
                {
                    return false;
                }
                --depth;
                ++m_p;
            }
            else if (c == ',' || c == ':')
            {
                if (depth == base)
                {
                    return false;
                }
                ++m_p;
            }
            else
            {
                const char* start = m_p;
                while (m_p < m_end && std::strchr(",:]} \t\r\n\"", *m_p) == nullptr)
                {
                    ++m_p;
                }
                if (m_p == start)
                {
                    return false;
                }
            }
        } while (depth > base);
        return true;
    }

    bool token(std::string_view& out)
    {
        skip_ws();
        const char* start = m_p;
        while (m_p < m_end && ((*m_p >= '0' && *m_p <= '9') || *m_p == '-' || *m_p == '+' || *m_p == '.' || *m_p == 'e' || *m_p == 'E'))
        {
            ++m_p;
        }
        out = std::string_view(start, static_cast<size_t>(m_p - start));
        return m_p > start;
    }

    bool literal(const char* word)
    {
        size_t length = std::strlen(word);
        skip_ws();
        if (static_cast<size_t>(m_end - m_p) < length || std::memcmp(m_p, word, length) != 0)
        {
            return false;
        }
        m_p += length;
        return true;
    }

    template<typename T>
    bool parse_int(T& value)
    {
        std::string_view text;
        return token(text) && convert(text, value);
    }

    // null is how the writer spells a non-finite number.
    bool parse_double(double& value)
    {
        if (literal("null"))
        {
            value = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        std::string_view text;
        return token(text) && convert(text, value);
    }

    bool parse_spaces(uint32_t* spaces, uint32_t& count)
    {
        if (!open_container('['))
        {
            return false;
        }
        count = 0;
        bool first = true;
        for (;;)
        {
            Next next = next_item(first, ']');
            first = false;
            if (next == Next::End)
            {
                return true;
            }
            uint32_t value = 0;
            if (next == Next::Error || !parse_int(value))
            {
                return false;
            }
            if (count < k_record_spaces)
            {
                spaces[count++] = value;
            }
        }
    }

    bool parse_players(std::vector<std::string>& players)
    {
        if (!open_container('['))
        {
            return false;
        }
        bool first = true;
        for (;;)
        {
            Next next = next_item(first, ']');
            first = false;
            if (next == Next::End)
            {
                return true;
            }
            Text text;
            if (next == Next::Error || !parse_string(text))
            {
                return false;
            }
            players.emplace_back(text.view);
        }
    }

    bool parse_params(Params& params)
    {
        if (!open_container('{'))
        {
            return false;
        }
        bool first = true;
        Text key;
        for (;;)
        {
            Next next = next_item(first, '}');
            first = false;
            if (next == Next::End)
            {
                return true;
            }
            if (next == Next::Error || !parse_string(key) || !consume(':'))
            {
                return false;
            }
            std::string name(key.view);
            skip_ws();
            if (m_p >= m_end)
            {
                return false;
            }
            if (literal("true"))
            {
                params[name] = true;
            }
            else if (literal("false"))
            {
                params[name] = false;
            }
            else if (*m_p == '"')
            {
                Text text;
                if (!parse_string(text))
                {
                    return false;
                }
                params[name] = std::string(text.view);
            }
            else if (*m_p == '-' || (*m_p >= '0' && *m_p <= '9'))
            {
                std::string_view text;
                int64_t integer = 0;
                double real = 0.0;
                if (!token(text))
                {
                    return false;
                }
                if (convert(text, integer))
                {
                    params[name] = integer;
                }
                else if (convert(text, real))
                {
                    params[name] = real;
                }
                else
                {
                    return false;
                }
            }
            else if (!skip_value())
            {
                return false;
            }
        }
    }

    void observe_chosen(const ScoreEntry& score)
    {
        DashboardRecord& record = m_out->record;
        if (!m_chosen_seen || m_chosen_done || score.action != record.chosen)
        {
            return;
        }
        m_chosen_done = true;
        if (score.has_value)
        {
            record.chosen_value = static_cast<float>(score.value);
        }
        if (score.has_probability)
        {
            record.chosen_probability = static_cast<float>(score.probability);
        }
    }

    // `chosen` may follow `scores` in a hand-written log; only the kept scores can be revisited then.
    void finish_chosen()
    {
        if (m_chosen_seen && !m_chosen_done)
        {
            for (uint32_t i = 0; i < m_out->record.score_count && !m_chosen_done; ++i)
            {
                observe_chosen(m_out->scores[i]);
            }
        }
    }

    bool parse_score(ScoreEntry& score)
    {
        if (!open_container('{'))
        {
            return false;
        }
        bool first = true;
        Text key;
        for (;;)
        {
            Next next = next_item(first, '}');
            first = false;
            if (next == Next::End)
            {
                return true;
            }
            if (next == Next::Error || !parse_string(key) || !consume(':'))
            {
                return false;
            }
            bool ok = true;
            if (key.view == "action")
            {
                ok = parse_int(score.action);
            }
            else if (key.view == "label")
            {
                Text text;
                ok = parse_string(text);
                if (ok)
                {
                    set_label(score.label, text.view);
                }
            }
            else if (key.view == "spaces")
            {
                uint32_t count = 0;
                ok = parse_spaces(score.spaces, count);
                score.space_count = static_cast<uint8_t>(count);
            }
            else if (key.view == "probability")
            {
                ok = parse_double(score.probability);
                score.has_probability = ok;
            }
            else if (key.view == "value")
            {
                ok = parse_double(score.value);
                score.has_value = ok;
            }
            else
            {
                ok = skip_value();
            }
            if (!ok)
            {
                return false;
            }
        }
    }

    bool parse_scores()
    {
        if (!open_container('['))
        {
            return false;
        }
        DashboardRecord& record = m_out->record;
        uint32_t seen = 0;
        bool first = true;
        for (;;)
        {
            Next next = next_item(first, ']');
            first = false;
            if (next == Next::End)
            {
                break;
            }
            ScoreEntry score;
            if (next == Next::Error || !parse_score(score))
            {
                return false;
            }
            observe_chosen(score);
            if (seen < m_out->score_capacity)
            {
                m_out->scores[seen] = score;
                ++record.score_count;
            }
            else
            {
                ++record.dropped_scores;
            }
            ++seen;
        }
        return true;
    }

    bool parse_extra()
    {
        if (!open_container('{'))
        {
            return false;
        }
        DashboardRecord& record = m_out->record;
        bool first = true;
        Text key;
        for (;;)
        {
            Next next = next_item(first, '}');
            first = false;
            if (next == Next::End)
            {
                return true;
            }
            if (next == Next::Error || !parse_string(key) || !consume(':'))
            {
                return false;
            }
            double value = 0.0;
            if (!parse_double(value))
            {
                return false;
            }
            int64_t id = key.truncated ? -1 : intern(key.view);
            if (id < 0 || record.metric_count >= m_out->metric_capacity)
            {
                ++record.dropped_metrics;
                continue;
            }
            m_out->metrics[record.metric_count++] = MetricEntry{ static_cast<uint16_t>(id), value };
        }
    }

    int64_t intern(std::string_view name)
    {
        ReplayKeyTable::Ids::const_iterator found = m_keys->ids.find(name);
        if (found != m_keys->ids.end())
        {
            return found->second;
        }
        if (!m_keys->interning || m_keys->names.size() >= m_keys->max_keys)
        {
            return -1;
        }
        uint32_t id = static_cast<uint32_t>(m_keys->names.size());
        m_keys->names.emplace_back(name);
        KeyStats stats;
        stats.keeps_max = is_max_metric(m_keys->names.back());
        m_keys->stats.push_back(stats);
        m_keys->ids.emplace(m_keys->names.back(), id);
        return id;
    }

    bool parse_tree()
    {
        if (!open_container('['))
        {
            return false;
        }
        DashboardRecord& record = m_out->record;
        uint32_t seen = 0;
        bool first = true;
        Text key;
        for (;;)
        {
            Next next = next_item(first, ']');
            first = false;
            if (next == Next::End)
            {
                return true;
            }
            if (next == Next::Error || !open_container('{'))
            {
                return false;
            }
            SearchNode node;
            bool node_first = true;
            for (;;)
            {
                Next member_next = next_item(node_first, '}');
                node_first = false;
                if (member_next == Next::End)
                {
                    break;
                }
                if (member_next == Next::Error || !parse_string(key) || !consume(':'))
                {
                    return false;
                }
                bool ok = true;
                if (key.view == "parent")
                {
                    ok = parse_int(node.parent);
                }
                else if (key.view == "action")
                {
                    ok = parse_int(node.action);
                }
                else if (key.view == "visits")
                {
                    ok = parse_int(node.visits);
                }
                else if (key.view == "value")
                {
                    ok = parse_double(node.value);
                }
                else
                {
                    ok = skip_value();
                }
                if (!ok)
                {
                    return false;
                }
            }
            if (seen < m_out->tree_capacity)
            {
                m_out->tree[seen] = node;
                ++record.tree_count;
            }
            else
            {
                ++record.dropped_tree_nodes;
            }
            ++seen;
        }
    }

    const char* m_p;
    const char* m_end;
    uint32_t m_max_depth;
    uint32_t m_depth = 0;
    ParsedRecord* m_out = nullptr;
    ReplayKeyTable* m_keys = nullptr;
    bool m_chosen_seen = false;
    bool m_chosen_done = false;
    char m_decode[k_decode_capacity] = {};
    size_t m_decoded = 0;
    bool m_decode_truncated = false;
};

void strip_cr(std::string& line)
{
    if (!line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
}

bool blank(const std::string& line)
{
    return line.find_first_not_of(" \t\r\n") == std::string::npos;
}

} // namespace

ReplayLog::ReplayLog() = default;

ReplayLog::~ReplayLog()
{
    close();
}

void ReplayLog::close()
{
    if (m_file != nullptr)
    {
        std::fclose(m_file);
        m_file = nullptr;
    }
    m_header = ReplayHeader();
    m_offsets = std::vector<uint64_t>();
    m_match_starts = std::vector<uint64_t>();
    m_keys = ReplayKeyTable();
    m_scratch_metrics = std::vector<MetricEntry>();
    m_block = std::vector<char>();
    m_line = std::string();
    m_committed = 0;
    m_last_match = -1;
    m_longest_line = 0;
    m_malformed = 0;
    m_oversize = 0;
    m_read_errors = 0;
    m_max_scores = 0;
    m_max_metrics = 0;
    m_max_tree_nodes = 0;
    m_truncated = false;
    m_incomplete_tail = false;
    m_has_time = false;
    m_header_done = false;
}

void ReplayLog::open(const std::string& path, const ReplayOptions& options)
{
    close();
    m_options = options;
    m_options.max_depth = math::clamp<uint32_t>(m_options.max_depth, 8, 64);
    m_options.max_line_bytes = math::max<uint32_t>(m_options.max_line_bytes, 256);
    m_options.max_keys = math::clamp<int32_t>(m_options.max_keys, 1, k_max_feed_keys);
    m_file = std::fopen(path.c_str(), "rb");
    if (m_file == nullptr)
    {
        throw Error("Replay: cannot open " + path);
    }
    m_keys.max_keys = static_cast<size_t>(m_options.max_keys);
    m_scratch_metrics.assign(m_options.max_line_bytes / k_min_metric_bytes + 1, MetricEntry{});
    m_block.resize(k_index_block);

    unsigned char bom[3] = {};
    size_t got = std::fread(bom, 1, sizeof(bom), m_file);
    uint64_t start = got == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF ? 3 : 0;
    try
    {
        m_committed = start;
        m_keys.interning = true;
        index_from(start);
        m_keys.interning = false;
    }
    catch (...)
    {
        close();
        throw;
    }
    m_block.resize(k_read_chunk);
    m_block.shrink_to_fit();
}

void ReplayLog::refresh()
{
    if (m_file == nullptr)
    {
        return;
    }
    std::clearerr(m_file);
    m_block.resize(k_index_block);
    m_keys.interning = true;
    index_from(m_committed);
    m_keys.interning = false;
    m_block.resize(k_read_chunk);
    m_block.shrink_to_fit();
}

void ReplayLog::index_from(uint64_t offset)
{
    m_incomplete_tail = false;
    if (fseeko(m_file, static_cast<off_t>(offset), SEEK_SET) != 0)
    {
        throw Error("Replay: cannot seek the log");
    }
    uint64_t block_offset = offset;
    uint64_t line_start = offset;
    bool overflow = false;
    m_line.clear();
    for (;;)
    {
        size_t got = std::fread(m_block.data(), 1, m_block.size(), m_file);
        if (got == 0)
        {
            break;
        }
        size_t pos = 0;
        while (pos < got)
        {
            const char* from = m_block.data() + pos;
            const char* newline = static_cast<const char*>(std::memchr(from, '\n', got - pos));
            size_t chunk = newline != nullptr ? static_cast<size_t>(newline - from) : got - pos;
            if (!overflow)
            {
                if (m_line.size() + chunk > m_options.max_line_bytes)
                {
                    overflow = true;
                    m_line.clear();
                }
                else
                {
                    m_line.append(from, chunk);
                }
            }
            pos += chunk;
            if (newline == nullptr)
            {
                break;
            }
            ++pos;
            if (overflow)
            {
                if (!m_header_done)
                {
                    throw Error("Replay: the header line is longer than " + std::to_string(m_options.max_line_bytes) + " bytes");
                }
                ++m_oversize;
            }
            else
            {
                handle_line(line_start, m_line);
            }
            line_start = block_offset + pos;
            m_committed = line_start;
            m_line.clear();
            overflow = false;
            if (m_truncated)
            {
                m_line.reserve(m_longest_line + 2);
                return;
            }
        }
        block_offset += got;
    }
    if (!m_header_done)
    {
        throw Error(m_line.empty() && !overflow ? "Replay: the log is empty" : "Replay: the header line is incomplete");
    }
    m_incomplete_tail = overflow || !blank(m_line);
    m_line.clear();
    m_line.reserve(m_longest_line + 2);
}

void ReplayLog::handle_line(uint64_t offset, std::string& line)
{
    strip_cr(line);
    if (blank(line))
    {
        return;
    }
    ParsedRecord scratch;
    ScanInfo info;
    if (!m_header_done)
    {
        Scanner scanner(line.data(), line.data() + line.size(), m_options.max_depth);
        ReplayHeader header;
        ReplayKeyTable no_keys;
        LineKind kind = scanner.scan(scratch, no_keys, &header, info);
        if (kind != LineKind::Header)
        {
            throw Error("Replay: the first line is not a trace header");
        }
        if (info.schema_version < 1)
        {
            throw Error("Replay: the header has no valid schema_version");
        }
        if (info.schema_version > k_trace_schema_version)
        {
            throw Error("Replay: schema_version " + std::to_string(info.schema_version) + " is newer than the supported " + std::to_string(k_trace_schema_version));
        }
        m_header = std::move(header);
        m_header_done = true;
        return;
    }

    size_t keys_before = m_keys.names.size();
    scratch.metrics = m_scratch_metrics.data();
    scratch.metric_capacity = static_cast<uint32_t>(m_scratch_metrics.size());
    Scanner scanner(line.data(), line.data() + line.size(), m_options.max_depth);
    LineKind kind = scanner.scan(scratch, m_keys, nullptr, info);
    bool indexed = kind == LineKind::Decision;
    if (indexed && m_offsets.size() >= m_options.max_records)
    {
        m_truncated = true;
        indexed = false;
    }
    if (!indexed)
    {
        if (kind == LineKind::Bad)
        {
            ++m_malformed;
        }
        while (m_keys.names.size() > keys_before)
        {
            m_keys.ids.erase(m_keys.names.back());
            m_keys.names.pop_back();
            m_keys.stats.pop_back();
        }
        return;
    }

    const DashboardRecord& record = scratch.record;
    for (uint32_t i = 0; i < record.metric_count; ++i)
    {
        KeyStats& stats = m_keys.stats[scratch.metrics[i].key];
        double value = scratch.metrics[i].value;
        stats.sum += value;
        stats.max = stats.count == 0 ? value : math::max(stats.max, value);
        stats.last = value;
        ++stats.count;
    }
    m_max_scores = math::max(m_max_scores, record.score_count + record.dropped_scores);
    m_max_metrics = math::max(m_max_metrics, record.metric_count + record.dropped_metrics);
    m_max_tree_nodes = math::max(m_max_tree_nodes, record.tree_count + record.dropped_tree_nodes);
    m_has_time = m_has_time || info.has_time;
    m_longest_line = math::max(m_longest_line, line.size());
    if (m_offsets.empty() || info.match != m_last_match)
    {
        m_match_starts.push_back(m_offsets.size());
        m_last_match = info.match;
    }
    m_offsets.push_back(offset);
}

uint64_t ReplayLog::match_of(uint64_t index) const
{
    std::vector<uint64_t>::const_iterator above = std::upper_bound(m_match_starts.begin(), m_match_starts.end(), index);
    return above == m_match_starts.begin() ? 0 : static_cast<uint64_t>(above - m_match_starts.begin()) - 1;
}

bool ReplayLog::read(uint64_t index, ParsedRecord& out)
{
    if (m_file == nullptr || index >= m_offsets.size())
    {
        return false;
    }
    if (fseeko(m_file, static_cast<off_t>(m_offsets[index]), SEEK_SET) != 0)
    {
        ++m_read_errors;
        return false;
    }
    m_line.clear();
    size_t limit = m_line.capacity();
    bool complete = false;
    while (!complete)
    {
        size_t got = std::fread(m_block.data(), 1, m_block.size(), m_file);
        if (got == 0)
        {
            break;
        }
        const char* newline = static_cast<const char*>(std::memchr(m_block.data(), '\n', got));
        size_t chunk = newline != nullptr ? static_cast<size_t>(newline - m_block.data()) : got;
        if (m_line.size() + chunk > limit)
        {
            break;
        }
        m_line.append(m_block.data(), chunk);
        complete = newline != nullptr;
    }
    strip_cr(m_line);
    ScanInfo info;
    Scanner scanner(m_line.data(), m_line.data() + m_line.size(), m_options.max_depth);
    if (!complete || scanner.scan(out, m_keys, nullptr, info) != LineKind::Decision)
    {
        ++m_read_errors;
        return false;
    }
    return true;
}

} // namespace oryx
