#include "oxpch.h"
#include "Oryx/Debug/ConsoleCommands.h"

#include "Oryx/Core/Settings.h"
#include "Oryx/Debug/MemoryTracker.h"

namespace oryx
{

ConsoleCall::ConsoleCall(std::string_view line)
    : m_line(line)
{
    size_t at = 0;
    while (at < m_line.size())
    {
        while (at < m_line.size() && std::isspace(static_cast<unsigned char>(m_line[at])))
        {
            ++at;
        }
        const size_t start = at;
        while (at < m_line.size() && !std::isspace(static_cast<unsigned char>(m_line[at])))
        {
            ++at;
        }
        if (at > start)
        {
            m_words.emplace_back(m_line.data() + start, at - start);
        }
    }
}

std::string_view ConsoleCall::arg(size_t index) const
{
    if (index >= count())
    {
        throw Error("missing argument " + std::to_string(index + 1));
    }
    return m_words[index + 1];
}

int64_t ConsoleCall::integer(size_t index) const
{
    const std::string_view text = arg(index);
    int64_t value = 0;
    const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size())
    {
        throw Error("argument " + std::to_string(index + 1) + " is not a whole number: " + std::string(text));
    }
    return value;
}

double ConsoleCall::number(size_t index) const
{
    const std::string text(arg(index));
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0')
    {
        throw Error("argument " + std::to_string(index + 1) + " is not a number: " + text);
    }
    return value;
}

void ConsoleCall::print(const char* format, ...)
{
    char text[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    m_output += text;
    m_output += '\n';
}

namespace console
{

namespace
{

struct Command
{
    std::string name;
    std::string usage;
    std::string help;
    ConsoleFunction function = nullptr;
};

std::vector<Command>& table()
{
    static std::vector<Command> commands;
    return commands;
}

std::string_view first_word(std::string_view text)
{
    const size_t end = text.find_first_of(" \t");
    return end == std::string_view::npos ? text : text.substr(0, end);
}

const Command* find(std::string_view name)
{
    for (const Command& command : table())
    {
        if (command.name == name)
        {
            return &command;
        }
    }
    return nullptr;
}

std::vector<std::string_view> words_of(std::string_view text)
{
    std::vector<std::string_view> words;
    size_t at = 0;
    while (at < text.size())
    {
        while (at < text.size() && text[at] == ' ')
        {
            ++at;
        }
        const size_t start = at;
        while (at < text.size() && text[at] != ' ')
        {
            ++at;
        }
        if (at > start)
        {
            words.push_back(text.substr(start, at - start));
        }
    }
    return words;
}

void collect_choices(std::string_view usage_word, std::vector<std::string>& out)
{
    if (usage_word.size() < 3 || (usage_word.front() != '<' && usage_word.front() != '[') || usage_word.find('|') == std::string_view::npos)
    {
        return;
    }
    std::string_view inner = usage_word.substr(1, usage_word.size() - 2);
    while (!inner.empty())
    {
        const size_t bar = inner.find('|');
        out.emplace_back(inner.substr(0, bar));
        inner = bar == std::string_view::npos ? std::string_view() : inner.substr(bar + 1);
    }
}

// The setting paths that continue `partial`, each cut after the next '.' so a section completes before its keys do.
void collect_settings(std::string_view partial, std::vector<std::string>& out)
{
    for (const SettingsText::Entry& entry : describe_settings({}))
    {
        if (entry.key.compare(0, partial.size(), partial) != 0)
        {
            continue;
        }
        const size_t dot = entry.key.find('.', partial.size());
        out.push_back(dot == std::string::npos ? entry.key : entry.key.substr(0, dot + 1));
    }
}

std::string common_prefix(const std::vector<std::string>& items)
{
    std::string prefix = items.front();
    for (const std::string& item : items)
    {
        size_t same = 0;
        while (same < prefix.size() && same < item.size() && prefix[same] == item[same])
        {
            ++same;
        }
        prefix.resize(same);
    }
    return prefix;
}

} // namespace

void add_command(std::string_view usage, std::string_view help, ConsoleFunction function)
{
    Command command;
    command.name = std::string(first_word(usage));
    command.usage = std::string(usage);
    command.help = std::string(help);
    command.function = function;
    table().push_back(std::move(command));
    std::sort(table().begin(), table().end(), [](const Command& a, const Command& b) { return a.name < b.name; });
}

void list(std::vector<Entry>& out)
{
    for (const Command& command : table())
    {
        out.push_back({ command.name, command.usage, command.help });
    }
}

void add_alias(std::string_view alias, std::string_view target)
{
    const Command* original = find(target);
    if (original == nullptr)
    {
        OX_CORE_WARN("console alias '{}' names an unknown command '{}'", alias, target);
        return;
    }
    Command copy = *original;
    copy.name = std::string(alias);
    table().push_back(std::move(copy));
    std::sort(table().begin(), table().end(), [](const Command& a, const Command& b) { return a.name < b.name; });
}

ConsoleResult run(std::string_view line, IConsoleSource* source)
{
    ConsoleResult result;
    if (!available())
    {
        result.ok = false;
        result.output = "console commands not built\n";
        return result;
    }
    ConsoleCall call(line);
    if (call.name().empty())
    {
        return result;
    }
    try
    {
        if (const Command* command = find(call.name()))
        {
            command->function(call);
        }
        else if (source == nullptr || !source->execute(call))
        {
            throw Error("unknown command '" + std::string(call.name()) + "' (try help)");
        }
    }
    catch (const Error& error)
    {
        result.ok = false;
        call.print("error: %s", error.what());
    }
    result.output = call.output();
    result.clear = call.clear_requested();
    return result;
}

bool complete(std::string& line, std::vector<std::string>& options, const IConsoleSource* source)
{
    options.clear();
    const std::vector<std::string_view> words = words_of(line);
    const bool fresh_word = line.empty() || line.back() == ' ';
    const size_t index = fresh_word ? words.size() : words.size() - 1;
    const std::string partial = fresh_word ? std::string() : std::string(words.back());
    std::vector<std::string> candidates;
    if (index == 0)
    {
        for (const Command& command : table())
        {
            candidates.push_back(command.name);
        }
        if (source != nullptr)
        {
            source->names(candidates);
        }
    }
    else if (const Command* command = find(words[0]))
    {
        const std::vector<std::string_view> usage = words_of(command->usage);
        if (index < usage.size() && usage[index] == "<section.key>")
        {
            collect_settings(partial, candidates);
        }
        else if (index < usage.size() && usage[index] == "[section]")
        {
            for (const std::string& section : settings_sections())
            {
                candidates.push_back(section);
            }
        }
        else if (index < usage.size())
        {
            collect_choices(usage[index], candidates);
        }
    }
    for (const std::string& candidate : candidates)
    {
        if (candidate.compare(0, partial.size(), partial) == 0)
        {
            options.push_back(candidate);
        }
    }
    if (options.empty())
    {
        return false;
    }
    std::sort(options.begin(), options.end());
    options.erase(std::unique(options.begin(), options.end()), options.end());
    std::string replacement = common_prefix(options);
    if (options.size() == 1 && replacement.back() != '.')
    {
        replacement += ' ';
    }
    if (replacement.size() == partial.size())
    {
        return false;
    }
    line.resize(line.size() - partial.size());
    line += replacement;
    return true;
}

namespace
{

constexpr const char* k_level_names[] = { "trace", "debug", "info", "warning", "error", "critical", "off" };

OX_CONSOLE_COMMAND(console_help, "help [command]", "Lists the commands, or shows one command's usage")
{
    if (call.count() > 0)
    {
        const Command* command = find(call.arg(0));
        if (command == nullptr)
        {
            throw Error("unknown command '" + std::string(call.arg(0)) + "'");
        }
        call.print("%s - %s", command->usage.c_str(), command->help.c_str());
        return;
    }
    for (const Command& command : table())
    {
        call.print("%-28s %s", command.usage.c_str(), command.help.c_str());
    }
}

OX_CONSOLE_COMMAND(console_clear, "clear", "Empties the console list")
{
    call.request_clear();
}

OX_CONSOLE_COMMAND(console_log_level, "log.level <core|app> <level>", "Sets a logger's level (trace debug info warning error critical off)")
{
    const std::string_view which = call.arg(0);
    const std::string_view level = call.arg(1);
    if (which != "core" && which != "app")
    {
        throw Error("logger must be core or app");
    }
    int32_t found = -1;
    for (int32_t i = 0; i < static_cast<int32_t>(std::size(k_level_names)); ++i)
    {
        if (level == k_level_names[i] || (level == "warn" && i == 3))
        {
            found = i;
        }
    }
    if (found < 0)
    {
        throw Error("unknown level '" + std::string(level) + "'");
    }
    std::shared_ptr<spdlog::logger>& logger = which == "core" ? Log::get_core_logger() : Log::get_client_logger();
    logger->set_level(static_cast<spdlog::level::level_enum>(found));
    call.print("%s logger level: %s", std::string(which).c_str(), k_level_names[found]);
}

OX_CONSOLE_COMMAND(console_stats_memory, "stats.memory", "Prints the allocation census")
{
    if (!MemoryTracker::is_installed())
    {
        call.print("allocation census not linked in this executable");
        return;
    }
    const MemoryStats stats = MemoryTracker::snapshot();
    call.print("live %lld B, peak %lld B, %lld allocations, %lld frees", static_cast<long long>(stats.live_bytes), static_cast<long long>(stats.peak_live_bytes),
        static_cast<long long>(stats.allocation_count), static_cast<long long>(stats.deallocation_count));
}

} // namespace

} // namespace console

void ConsoleHistory::push(std::string_view line)
{
    m_cursor = 0;
    if (line.empty() || (m_count > 0 && at(1) == line))
    {
        return;
    }
    m_lines[m_head] = std::string(line);
    m_head = (m_head + 1) % k_console_history;
    m_count = m_count < k_console_history ? m_count + 1 : m_count;
}

std::string_view ConsoleHistory::at(uint32_t steps_back) const
{
    return m_lines[(m_head + k_console_history - steps_back) % k_console_history];
}

std::string_view ConsoleHistory::previous()
{
    if (m_cursor < m_count)
    {
        ++m_cursor;
    }
    return m_cursor == 0 ? std::string_view() : at(m_cursor);
}

std::string_view ConsoleHistory::next()
{
    if (m_cursor > 0)
    {
        --m_cursor;
    }
    return m_cursor == 0 ? std::string_view() : at(m_cursor);
}

OX_CONSOLE_COMMAND(console_settings_get, "settings.get <section.key>", "Prints a setting's value")
{
    call.print("%s = %s", std::string(call.arg(0)).c_str(), get_setting(call.arg(0)).c_str());
}

OX_CONSOLE_COMMAND(console_settings_set, "settings.set <section.key> <value>", "Applies a setting live (not saved; lists are comma-separated)")
{
    set_setting(call.arg(0), call.arg(1));
    call.print("%s = %s", std::string(call.arg(0)).c_str(), get_setting(call.arg(0)).c_str());
}

OX_CONSOLE_COMMAND(console_settings_list, "settings.list [section]", "Lists the settings of the reflected sections with their values")
{
    for (const SettingsText::Entry& entry : describe_settings(call.count() > 0 ? call.arg(0) : std::string_view()))
    {
        call.print("%s = %s%s%s", entry.key.c_str(), entry.text.c_str(), entry.doc.empty() ? "" : "  # ", entry.doc.c_str());
    }
}

} // namespace oryx
