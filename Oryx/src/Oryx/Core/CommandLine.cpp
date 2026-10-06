#include "CommandLine.h"

namespace oryx
{

bool ParsedArgs::has(std::string_view name) const
{
    return m_values.find(name) != m_values.end();
}

std::string ParsedArgs::value(std::string_view name) const
{
    return value_or(name, "");
}

std::string ParsedArgs::value_or(std::string_view name, std::string fallback) const
{
    auto found = m_values.find(name);
    return found == m_values.end() ? std::move(fallback) : found->second.back();
}

std::vector<std::string> ParsedArgs::values(std::string_view name) const
{
    auto found = m_values.find(name);
    return found == m_values.end() ? std::vector<std::string>{} : found->second;
}

CommandLine::CommandLine(std::string program, std::string summary)
    : m_program(std::move(program))
    , m_summary(std::move(summary))
{
}

CommandLine& CommandLine::flag(std::string name, std::string help, std::vector<std::string> aliases)
{
    m_specs.push_back({ std::move(name), "", std::move(help), std::move(aliases), false });
    return *this;
}

CommandLine& CommandLine::option(std::string name, std::string value_name, std::string help, std::vector<std::string> aliases)
{
    m_specs.push_back({ std::move(name), std::move(value_name), std::move(help), std::move(aliases), true });
    return *this;
}

CommandLine CommandLine::global(std::string program)
{
    CommandLine command_line(std::move(program));
    std::vector<std::string> names = CommandLineContributors::names();
    std::sort(names.begin(), names.end());
    for (const std::string& name : names)
    {
        CommandLineContributors::create(name)->declare(command_line);
    }
    return command_line;
}

const CommandLine::Spec* CommandLine::find(std::string_view name) const
{
    for (const Spec& spec : m_specs)
    {
        if (spec.name == name || std::find(spec.aliases.begin(), spec.aliases.end(), name) != spec.aliases.end())
        {
            return &spec;
        }
    }
    return nullptr;
}

ParsedArgs CommandLine::parse(const ApplicationCommandLineArgs& args) const
{
    std::vector<std::string> tokens;
    for (int32_t i = 1; i < args.count; ++i)
    {
        tokens.emplace_back(args[i]);
    }
    return parse(tokens);
}

ParsedArgs CommandLine::parse(const std::vector<std::string>& args) const
{
    ParsedArgs parsed;
    for (size_t i = 0; i < args.size(); ++i)
    {
        std::string_view arg = args[i];

        if (arg.substr(0, 2) != "--")
        {
            throw Error("Unexpected argument '" + std::string(arg) + "'");
        }

        std::string_view body = arg.substr(2);
        size_t equals = body.find('=');
        std::string_view name = body.substr(0, equals);
        bool has_inline_value = equals != std::string_view::npos;

        const Spec* spec = find(name);
        if (spec == nullptr)
        {
            throw Error("Unknown option '--" + std::string(name) + "'");
        }

        if (!spec->takes_value)
        {
            if (has_inline_value)
            {
                throw Error("Option '--" + spec->name + "' does not take a value");
            }
            parsed.m_values[spec->name].emplace_back();
            continue;
        }

        if (has_inline_value)
        {
            parsed.m_values[spec->name].emplace_back(body.substr(equals + 1));
        }
        else if (i + 1 < args.size())
        {
            parsed.m_values[spec->name].push_back(args[++i]);
        }
        else
        {
            throw Error("Option '--" + spec->name + "' needs a value (" + spec->value_name + ")");
        }
    }
    return parsed;
}

std::string CommandLine::usage() const
{
    std::string text = "Usage: " + m_program + " [options]\n";
    if (!m_summary.empty())
    {
        text += m_summary + "\n";
    }
    text += "\nOptions:\n";
    for (const Spec& spec : m_specs)
    {
        std::string left = "  --" + spec.name;
        if (spec.takes_value)
        {
            left += "=<" + spec.value_name + ">";
        }
        for (const std::string& alias : spec.aliases)
        {
            left += ", --" + alias;
        }
        text += left + "\n      " + spec.help + "\n";
    }
    return text;
}

} // namespace oryx
