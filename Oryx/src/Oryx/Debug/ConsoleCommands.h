#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"

// Named commands the in-app console runs, each a plain function over a ConsoleCall. Register one with a single macro line at namespace scope:
//   OX_CONSOLE_COMMAND(log_level, "log.level <core|app> <level>", "Sets a logger's level") { ... call.arg(0) ... }
//   OX_REGISTER_CONSOLE_COMMAND(existing_function, "name [arg]", "What it does")
// The usage string's first word is the command name; `<a|b>` and `[a|b]` words complete as choices. A failure throws Error, which the console prints. Without OX_ENABLE_CONSOLE_COMMANDS (the log_capture forge option) the macros still compile
// the function but register nothing, and the console answers "console commands not built".
namespace oryx
{

inline constexpr uint32_t k_console_history = 64;

class ConsoleCall
{
public:
    explicit ConsoleCall(std::string_view line);

    [[nodiscard]] std::string_view name() const { return m_words.empty() ? std::string_view() : m_words[0]; }
    // Arguments after the command name.
    [[nodiscard]] size_t count() const { return m_words.empty() ? 0 : m_words.size() - 1; }
    // Each throws Error when argument `index` is missing; integer and number throw when it is not a number.
    [[nodiscard]] std::string_view arg(size_t index) const;
    [[nodiscard]] int64_t integer(size_t index) const;
    [[nodiscard]] double number(size_t index) const;

    void print(const char* format, ...) __attribute__((format(printf, 2, 3)));
    // The console empties its list after this call.
    void request_clear() { m_clear = true; }

    [[nodiscard]] const std::string& output() const { return m_output; }
    [[nodiscard]] bool clear_requested() const { return m_clear; }

private:
    std::string m_line;
    std::vector<std::string_view> m_words;
    std::string m_output;
    bool m_clear = false;
};

using ConsoleFunction = void (*)(ConsoleCall& call);

// Commands that live outside the table (a workbench's, for instance): the console asks it after the table.
class IConsoleSource
{
public:
    virtual ~IConsoleSource() = default;

    // True when the call's name is one of this source's; it then ran.
    virtual bool execute(ConsoleCall& call) = 0;
    virtual void names(std::vector<std::string>& out) const = 0;
};

struct ConsoleResult
{
    std::string output;
    bool ok = true;
    bool clear = false;
};

namespace console
{

[[nodiscard]] inline bool available()
{
#ifdef OX_ENABLE_CONSOLE_COMMANDS
    return true;
#else
    return false;
#endif
}

struct Entry
{
    std::string name;
    std::string usage;
    std::string help;
};

// Every registered command and alias by name, for a palette or help listing.
void list(std::vector<Entry>& out);

void add_command(std::string_view usage, std::string_view help, ConsoleFunction function);
void add_alias(std::string_view alias, std::string_view target);

// Runs one line; an Error from the command is printed into the output and sets ok to false.
[[nodiscard]] ConsoleResult run(std::string_view line, IConsoleSource* source = nullptr);

// Completes the last word of `line` against command names (first word) or the usage's choices (later words): one match replaces it, several extend it to their common prefix and are listed in `options`. True when `line` changed.
bool complete(std::string& line, std::vector<std::string>& options, const IConsoleSource* source = nullptr);

class Registration
{
public:
    Registration(std::string_view usage, std::string_view help, ConsoleFunction function) { add_command(usage, help, function); }
    Registration(std::string_view alias, std::string_view target) { add_alias(alias, target); }
};

} // namespace console

// The lines run so far, newest last; previous/next walk them and an empty view means the fresh line.
class ConsoleHistory
{
public:
    // Blank lines and a repeat of the newest line are not kept.
    void push(std::string_view line);
    [[nodiscard]] std::string_view previous();
    [[nodiscard]] std::string_view next();
    void reset_cursor() { m_cursor = 0; }
    [[nodiscard]] uint32_t size() const { return m_count; }

private:
    [[nodiscard]] std::string_view at(uint32_t steps_back) const;

    std::string m_lines[k_console_history];
    uint32_t m_head = 0;
    uint32_t m_count = 0;
    uint32_t m_cursor = 0;
};

} // namespace oryx

#ifdef OX_ENABLE_CONSOLE_COMMANDS

#define OX_REGISTER_CONSOLE_COMMAND(function, usage, help) \
    namespace                                              \
    {                                                      \
    [[maybe_unused]] const ::oryx::console::Registration OX_CONCAT(g_ox_console_, __LINE__)(usage, help, function); \
    }

#define OX_CONSOLE_COMMAND_ALIAS(alias, target) \
    namespace                                   \
    {                                           \
    [[maybe_unused]] const ::oryx::console::Registration OX_CONCAT(g_ox_console_, __LINE__)(alias, target); \
    }

#else

#define OX_REGISTER_CONSOLE_COMMAND(function, usage, help)
#define OX_CONSOLE_COMMAND_ALIAS(alias, target)

#endif

#define OX_CONSOLE_COMMAND(function, usage, help)                          \
    [[maybe_unused]] static void function(::oryx::ConsoleCall& call);      \
    OX_REGISTER_CONSOLE_COMMAND(function, usage, help)                     \
    [[maybe_unused]] static void function([[maybe_unused]] ::oryx::ConsoleCall& call)
