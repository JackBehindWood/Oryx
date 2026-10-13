#include "oxpch.h"
#include "Oryx/Core/Settings.h"

namespace oryx
{

std::string check_range(const FieldInfo& info, int64_t value)
{
    if (value >= info.min && value <= info.max)
    {
        return {};
    }
    return "must be between " + std::to_string(info.min) + " and " + std::to_string(info.max) + ", got " + std::to_string(value);
}

std::string check_choice(const FieldInfo& info, std::string_view name)
{
    std::string allowed;
    for (uint32_t i = 0; i < info.choice_count; ++i)
    {
        if (info.choices[i] == name)
        {
            return {};
        }
        allowed += (allowed.empty() ? "" : ", ") + std::string(info.choices[i]);
    }
    return "must be one of " + allowed + ", got '" + std::string(name) + "'";
}

namespace
{

int32_t choice_index(const FieldInfo& info, std::string_view name)
{
    for (uint32_t i = 0; i < info.choice_count; ++i)
    {
        if (info.choices[i] == name)
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

bool whole_number(std::string_view text, int64_t& value)
{
    const char* end = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(text.data(), end, value);
    return !text.empty() && result.ec == std::errc() && result.ptr == end;
}

std::string_view trimmed(std::string_view text)
{
    while (!text.empty() && text.front() == ' ')
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ')
    {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

SettingsReader::SettingsReader(const SettingsNode& node)
    : m_node(node)
{
}

void SettingsReader::field(const FieldInfo& info, bool& value)
{
    value = m_node.boolean(info.key, value);
}

void SettingsReader::field(const FieldInfo& info, int32_t& value)
{
    const std::string where = std::string(m_node.section()) + "." + std::string(info.key) + " ";
    if (info.choice_count > 0)
    {
        if (m_node.has(info.key))
        {
            const std::string name = m_node.string(info.key);
            const int32_t index = choice_index(info, name);
            if (index < 0)
            {
                throw SettingsError(where + check_choice(info, name));
            }
            value = index;
        }
        return;
    }
    const int64_t read = m_node.integer(info.key, value);
    if (const std::string message = check_range(info, read); !message.empty())
    {
        throw SettingsError(where + message);
    }
    value = static_cast<int32_t>(read);
}

void SettingsReader::field(const FieldInfo& info, uint32_t& value)
{
    const int64_t read = m_node.integer(info.key, value);
    if (const std::string message = check_range(info, read); !message.empty())
    {
        throw SettingsError(std::string(m_node.section()) + "." + std::string(info.key) + " " + message);
    }
    value = static_cast<uint32_t>(read);
}

void SettingsReader::field(const FieldInfo& info, float& value)
{
    const std::string where = std::string(m_node.section()) + "." + std::string(info.key) + " ";
    if (info.percent)
    {
        const int64_t read = m_node.integer(info.key, static_cast<int64_t>(std::lround(value * 100.0f)));
        if (const std::string message = check_range(info, read); !message.empty())
        {
            throw SettingsError(where + message);
        }
        value = static_cast<float>(read) / 100.0f;
        return;
    }
    if (m_node.has(info.key))
    {
        const std::string text = m_node.string(info.key);
        char* end = nullptr;
        const float read = std::strtof(text.c_str(), &end);
        if (text.empty() || end != text.c_str() + text.size())
        {
            throw SettingsError(where + "must be a number, got '" + text + "'");
        }
        value = read;
    }
}

void SettingsReader::field(const FieldInfo& info, std::string& value)
{
    value = m_node.string(info.key, value);
}

void SettingsReader::field(const FieldInfo& info, std::filesystem::path& value)
{
    value = m_node.has(info.key) && m_node.string(info.key).empty() ? std::filesystem::path() : m_node.path(info.key, value);
}

void SettingsReader::field(const FieldInfo& info, std::vector<std::string>& value)
{
    if (!m_node.has(info.key))
    {
        return;
    }
    value = m_node.strings(info.key);
    if (info.choice_count == 0)
    {
        return;
    }
    for (const std::string& name : value)
    {
        if (const std::string message = check_choice(info, name); !message.empty())
        {
            throw SettingsError(std::string(m_node.section()) + "." + std::string(info.key) + " " + message);
        }
    }
}

void SettingsReader::removed(std::string_view key, std::string_view message)
{
    if (m_node.has(key))
    {
        throw SettingsError(std::string(m_node.section()) + "." + std::string(key) + " was removed: " + std::string(message));
    }
}

namespace
{

std::string format(const FieldInfo&, bool value) { return value ? "true" : "false"; }
std::string format(const FieldInfo& info, int32_t value) { return info.choice_count > 0 ? std::string(info.choices[math::clamp<int32_t>(value, 0, static_cast<int32_t>(info.choice_count) - 1)]) : std::to_string(value); }
std::string format(const FieldInfo&, uint32_t value) { return std::to_string(value); }
std::string format(const FieldInfo& info, float value)
{
    if (info.percent)
    {
        return std::to_string(std::lround(value * 100.0f));
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
    return buffer;
}
std::string format(const FieldInfo&, const std::string& value) { return value; }
std::string format(const FieldInfo&, const std::filesystem::path& value) { return value.generic_string(); }
std::string format(const FieldInfo&, const std::vector<std::string>& value)
{
    std::string text;
    for (const std::string& item : value)
    {
        text += (text.empty() ? "" : ",") + item;
    }
    return text;
}

std::string parse(const FieldInfo&, std::string_view text, bool& value)
{
    if (text == "true" || text == "1")
    {
        value = true;
    }
    else if (text == "false" || text == "0")
    {
        value = false;
    }
    else
    {
        return "must be true or false, got '" + std::string(text) + "'";
    }
    return {};
}

std::string parse(const FieldInfo& info, std::string_view text, int32_t& value)
{
    if (info.choice_count > 0)
    {
        if (std::string message = check_choice(info, text); !message.empty())
        {
            return message;
        }
        value = choice_index(info, text);
        return {};
    }
    int64_t number = 0;
    if (!whole_number(text, number))
    {
        return "must be an integer, got '" + std::string(text) + "'";
    }
    if (std::string message = check_range(info, number); !message.empty())
    {
        return message;
    }
    value = static_cast<int32_t>(number);
    return {};
}

std::string parse(const FieldInfo& info, std::string_view text, uint32_t& value)
{
    int64_t number = 0;
    if (!whole_number(text, number))
    {
        return "must be an integer, got '" + std::string(text) + "'";
    }
    if (std::string message = check_range(info, number); !message.empty())
    {
        return message;
    }
    value = static_cast<uint32_t>(number);
    return {};
}

std::string parse(const FieldInfo& info, std::string_view text, float& value)
{
    if (info.percent)
    {
        int64_t number = 0;
        if (!whole_number(text, number))
        {
            return "must be a whole number, got '" + std::string(text) + "'";
        }
        if (std::string message = check_range(info, number); !message.empty())
        {
            return message;
        }
        value = static_cast<float>(number) / 100.0f;
        return {};
    }
    const std::string copy(text);
    char* end = nullptr;
    const float read = std::strtof(copy.c_str(), &end);
    if (copy.empty() || end != copy.c_str() + copy.size())
    {
        return "must be a number, got '" + copy + "'";
    }
    value = read;
    return {};
}

std::string parse(const FieldInfo&, std::string_view text, std::string& value)
{
    value = std::string(text);
    return {};
}

std::string parse(const FieldInfo&, std::string_view text, std::filesystem::path& value)
{
    value = std::filesystem::path(std::string(text));
    return {};
}

std::string parse(const FieldInfo& info, std::string_view text, std::vector<std::string>& value)
{
    std::vector<std::string> items;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find(',', start);
        end = end == std::string_view::npos ? text.size() : end;
        const std::string_view item = trimmed(text.substr(start, end - start));
        if (!item.empty())
        {
            if (info.choice_count > 0)
            {
                if (std::string message = check_choice(info, item); !message.empty())
                {
                    return message;
                }
            }
            items.emplace_back(item);
        }
        start = end + 1;
    }
    value = std::move(items);
    return {};
}

} // namespace

SettingsText::SettingsText(Mode mode, std::string_view key, std::string_view text)
    : m_mode(mode)
    , m_key(key)
    , m_text(text)
{
}

template<typename T>
void SettingsText::visit(const FieldInfo& info, T& value, SettingsKind kind)
{
    if (m_mode == Mode::Collect)
    {
        Entry entry;
        entry.key = std::string(info.key);
        entry.text = format(info, value);
        entry.doc = std::string(info.doc);
        entry.kind = kind;
        entry.list = kind == SettingsKind::List;
        for (uint32_t i = 0; i < info.choice_count; ++i)
        {
            entry.choices.emplace_back(info.choices[i]);
        }
        m_entries.push_back(std::move(entry));
        return;
    }
    if (info.key != m_key)
    {
        return;
    }
    m_found = true;
    if (m_mode == Mode::Get)
    {
        m_result = format(info, value);
        return;
    }
    T parsed = value;
    m_error = parse(info, m_text, parsed);
    if (m_error.empty())
    {
        value = std::move(parsed);
    }
}

void SettingsText::field(const FieldInfo& info, bool& value) { visit(info, value, SettingsKind::Bool); }
void SettingsText::field(const FieldInfo& info, int32_t& value) { visit(info, value, info.choice_count > 0 ? SettingsKind::Choice : SettingsKind::Integer); }
void SettingsText::field(const FieldInfo& info, uint32_t& value) { visit(info, value, SettingsKind::Integer); }
void SettingsText::field(const FieldInfo& info, float& value) { visit(info, value, info.percent ? SettingsKind::Integer : SettingsKind::Number); }
void SettingsText::field(const FieldInfo& info, std::string& value) { visit(info, value, SettingsKind::Text); }
void SettingsText::field(const FieldInfo& info, std::filesystem::path& value) { visit(info, value, SettingsKind::Text); }
void SettingsText::field(const FieldInfo& info, std::vector<std::string>& value) { visit(info, value, SettingsKind::List); }

namespace
{

void split_path(std::string_view path, std::string& section, std::string& key)
{
    const size_t dot = path.find('.');
    if (dot == std::string_view::npos || dot == 0 || dot + 1 == path.size())
    {
        throw SettingsError("a setting is named <section>.<key>, got '" + std::string(path) + "'");
    }
    section = std::string(path.substr(0, dot));
    key = std::string(path.substr(dot + 1));
}

} // namespace

std::string get_setting(std::string_view path)
{
    std::string section;
    std::string key;
    split_path(path, section, key);
    SharedPtr<void> copy = settings_copy(section);
    SettingsText text(SettingsText::Mode::Get, key);
    walk_settings(section, copy.get(), text);
    if (!text.found())
    {
        throw SettingsError("no setting '" + std::string(path) + "'");
    }
    return text.result();
}

void set_setting(std::string_view path, std::string_view value)
{
    std::string section;
    std::string key;
    split_path(path, section, key);
    SharedPtr<void> copy = settings_copy(section);
    SettingsText text(SettingsText::Mode::Set, key, value);
    walk_settings(section, copy.get(), text);
    if (!text.found())
    {
        throw SettingsError("no setting '" + std::string(path) + "'");
    }
    if (!text.error().empty())
    {
        throw SettingsError(std::string(path) + " " + text.error());
    }
    commit_settings(section, copy.get());
}

std::vector<SettingsText::Entry> describe_settings(std::string_view section)
{
    std::vector<SettingsText::Entry> result;
    for (const std::string& name : settings_sections())
    {
        if (!section.empty() && name != section)
        {
            continue;
        }
        SharedPtr<void> copy = settings_copy(name);
        SettingsText text;
        walk_settings(name, copy.get(), text);
        for (SettingsText::Entry entry : text.entries())
        {
            entry.key = name + "." + entry.key;
            result.push_back(std::move(entry));
        }
    }
    return result;
}

} // namespace oryx
