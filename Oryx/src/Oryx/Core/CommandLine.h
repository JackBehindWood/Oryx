#pragma once

#include "Oryx/Core/Application.h"
#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Core/Registry.h"

namespace oryx
{

class ParsedArgs
{
public:
    [[nodiscard]] bool has(std::string_view name) const;
    // The last value given; empty when absent or for a flag.
    [[nodiscard]] std::string value(std::string_view name) const;
    [[nodiscard]] std::string value_or(std::string_view name, std::string fallback) const;
    // Every value given, in order, for an option that may repeat.
    [[nodiscard]] std::vector<std::string> values(std::string_view name) const;

private:
    friend class CommandLine;
    std::map<std::string, std::vector<std::string>, std::less<>> m_values;
};

// Declarative `--name`, `--name=value` and `--name value` options; names given to the declaration are canonical, aliases resolve to them.
class CommandLine
{
public:
    explicit CommandLine(std::string program, std::string summary = "");

    CommandLine& flag(std::string name, std::string help, std::vector<std::string> aliases = {});
    CommandLine& option(std::string name, std::string value_name, std::string help, std::vector<std::string> aliases = {});
    // Every contributor registered with OX_REGISTER_COMMAND_LINE, in name order.
    [[nodiscard]] static CommandLine global(std::string program = "oryx");

    // Throws Error on an unknown option, a missing value or a value given to a flag. argv[0] is skipped.
    [[nodiscard]] ParsedArgs parse(const ApplicationCommandLineArgs& args) const;
    [[nodiscard]] ParsedArgs parse(const std::vector<std::string>& args) const;
    [[nodiscard]] std::string usage() const;

private:
    struct Spec
    {
        std::string name;
        std::string value_name;
        std::string help;
        std::vector<std::string> aliases;
        bool takes_value = false;
    };

    [[nodiscard]] const Spec* find(std::string_view name) const;

    std::string m_program;
    std::string m_summary;
    std::vector<Spec> m_specs;
};

// What one subsystem adds to the command line; its typed reader (e.g. script_options) turns the ParsedArgs back into its own settings.
class ICommandLineContributor
{
public:
    virtual ~ICommandLineContributor() = default;

    virtual void declare(CommandLine& command_line) const = 0;
};

using CommandLineContributors = Registry<ICommandLineContributor>;

} // namespace oryx

// `name` is the contributing subsystem, which also orders the options in usage().
#define OX_REGISTER_COMMAND_LINE(Type, name) \
    OX_REGISTER_FACTORY(::oryx::ICommandLineContributor, Type, name)
