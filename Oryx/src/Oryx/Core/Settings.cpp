#include "oxpch.h"
#include "Oryx/Core/Settings.h"

#include "Oryx/Core/Application.h"

#include <yaml-cpp/yaml.h>

namespace oryx
{

struct SettingsNode::Impl
{
    YAML::Node node;
    std::filesystem::path file;
    std::string section;
    mutable std::vector<std::string> read;
};

struct SettingsAccess
{
    static SettingsNode make(const YAML::Node& node, const std::filesystem::path& file, const std::string& section)
    {
        return SettingsNode(create_unique<SettingsNode::Impl>(SettingsNode::Impl{ node, file, section, {} }));
    }

    static YAML::Node child(const SettingsNode& node, std::string_view key)
    {
        const SettingsNode::Impl& impl = *node.m_impl;
        impl.read.emplace_back(key);
        const YAML::Node& mapping = impl.node;
        return mapping[std::string(key)];
    }

    [[noreturn]] static void fail(const SettingsNode& node, std::string_view key, const YAML::Node& value, const std::string& reason)
    {
        const SettingsNode::Impl& impl = *node.m_impl;
        std::string where = impl.file.string();
        if (value.IsDefined() && value.Mark().line >= 0)
        {
            where += ":" + std::to_string(value.Mark().line + 1);
        }
        throw SettingsError(where + ": " + impl.section + "." + std::string(key) + " " + reason);
    }

    static const std::filesystem::path& file_of(const SettingsNode& node) { return node.m_impl->file; }

    static void warn_unknown_keys(const SettingsNode& node)
    {
        const SettingsNode::Impl& impl = *node.m_impl;
        if (!impl.node.IsMap())
        {
            return;
        }

        for (const auto& entry : impl.node)
        {
            std::string key = entry.first.as<std::string>();
            if (std::find(impl.read.begin(), impl.read.end(), key) == impl.read.end())
            {
                OX_CORE_WARN("Settings: unknown key '{}' in section '{}' ({}:{})", key, impl.section, impl.file.string(), entry.first.Mark().line + 1);
            }
        }
    }
};

SettingsNode::SettingsNode(UniquePtr<Impl> impl)
    : m_impl(std::move(impl))
{
}

SettingsNode::~SettingsNode() = default;

bool SettingsNode::has(std::string_view key) const
{
    return SettingsAccess::child(*this, key).IsDefined();
}

std::string SettingsNode::string(std::string_view key, std::string fallback) const
{
    YAML::Node value = SettingsAccess::child(*this, key);
    if (!value.IsDefined())
    {
        return fallback;
    }
    if (!value.IsScalar())
    {
        SettingsAccess::fail(*this, key, value, "must be a string");
    }
    return value.as<std::string>();
}

int64_t SettingsNode::integer(std::string_view key, int64_t fallback) const
{
    YAML::Node value = SettingsAccess::child(*this, key);
    if (!value.IsDefined())
    {
        return fallback;
    }

    try
    {
        return value.as<int64_t>();
    }
    catch (const YAML::Exception&)
    {
        SettingsAccess::fail(*this, key, value, "must be an integer");
    }
}

bool SettingsNode::boolean(std::string_view key, bool fallback) const
{
    YAML::Node value = SettingsAccess::child(*this, key);
    if (!value.IsDefined())
    {
        return fallback;
    }

    try
    {
        return value.as<bool>();
    }
    catch (const YAML::Exception&)
    {
        SettingsAccess::fail(*this, key, value, "must be true or false");
    }
}

std::vector<std::string> SettingsNode::strings(std::string_view key) const
{
    YAML::Node value = SettingsAccess::child(*this, key);
    if (!value.IsDefined())
    {
        return {};
    }
    if (!value.IsSequence())
    {
        SettingsAccess::fail(*this, key, value, "must be a list of strings");
    }

    std::vector<std::string> result;
    for (const YAML::Node& item : value)
    {
        if (!item.IsScalar())
        {
            SettingsAccess::fail(*this, key, item, "must be a list of strings");
        }
        result.push_back(item.as<std::string>());
    }
    return result;
}

std::filesystem::path SettingsNode::path(std::string_view key, std::filesystem::path fallback) const
{
    std::string text = string(key);
    if (text.empty())
    {
        return fallback;
    }

    std::filesystem::path result(text);
    return result.is_absolute() ? result : (SettingsAccess::file_of(*this).parent_path() / result).lexically_normal();
}

std::vector<std::filesystem::path> SettingsNode::paths(std::string_view key) const
{
    std::vector<std::filesystem::path> result;
    for (const std::string& text : strings(key))
    {
        std::filesystem::path item(text);
        result.push_back(item.is_absolute() ? item : (SettingsAccess::file_of(*this).parent_path() / item).lexically_normal());
    }
    return result;
}

namespace
{

constexpr std::string_view kSettingsFlag = "--settings=";
constexpr const char* kDefaultFileName = "oryx.yaml";

struct Section
{
    std::string name;
    std::type_index type;
    SharedPtr<void> value;
    SettingsBuilder build;
    SettingsAssigner assign;
};

struct State
{
    std::vector<Section> sections;
    std::filesystem::path file;
    bool file_required = false;
    std::vector<std::filesystem::path> default_files;
};

State& state()
{
    static State instance;
    return instance;
}

// False when the optional default file does not exist.
bool read_root(const std::filesystem::path& file, bool required, YAML::Node& root)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error))
    {
        if (required)
        {
            throw SettingsError("settings file not found: " + file.string());
        }
        return false;
    }

    try
    {
        root = YAML::LoadFile(file.string());
        return true;
    }
    catch (const YAML::Exception& exception)
    {
        throw SettingsError(file.string() + ":" + std::to_string(exception.mark.line + 1) + ": " + exception.msg);
    }
}

void apply(const std::filesystem::path& file, bool required)
{
    State& current = state();
    YAML::Node root;
    bool exists = read_root(file, required, root);
    if (!root.IsNull() && !root.IsMap())
    {
        throw SettingsError(file.string() + ": the settings file must be a mapping of section names");
    }

    const YAML::Node& view = root;
    std::vector<SharedPtr<void>> staged;
    staged.reserve(current.sections.size());
    for (const Section& section : current.sections)
    {
        YAML::Node node = view.IsMap() ? view[section.name] : YAML::Node();
        if (!node.IsDefined())
        {
            staged.push_back(section.build(nullptr));
            continue;
        }
        if (!node.IsNull() && !node.IsMap())
        {
            throw SettingsError(file.string() + ":" + std::to_string(node.Mark().line + 1) + ": section '" + section.name + "' must be a mapping");
        }

        SettingsNode reader = SettingsAccess::make(node, file, section.name);
        staged.push_back(section.build(&reader));
        SettingsAccess::warn_unknown_keys(reader);
    }

    if (root.IsMap())
    {
        for (const auto& entry : root)
        {
            std::string name = entry.first.as<std::string>();
            bool known = std::any_of(current.sections.begin(), current.sections.end(), [&name](const Section& section) { return section.name == name; });
            if (!known)
            {
                OX_CORE_WARN("Settings: unknown section '{}' in {}:{}", name, file.string(), entry.first.Mark().line + 1);
            }
        }
    }

    for (size_t i = 0; i < current.sections.size(); ++i)
    {
        current.sections[i].assign(current.sections[i].value.get(), staged[i].get());
    }
    current.file = file;
    current.file_required = required;
    if (exists)
    {
        OX_CORE_INFO("Settings: loaded {}", file.string());
    }
    else
    {
        OX_CORE_TRACE("Settings: no {}, every section keeps its defaults", file.string());
    }
}

} // namespace

void register_settings_section(std::string name, std::type_index type, SharedPtr<void> value, SettingsBuilder build, SettingsAssigner assign)
{
    state().sections.push_back(Section{ std::move(name), type, std::move(value), std::move(build), assign });
}

const void* settings_value(std::type_index type)
{
    for (const Section& section : state().sections)
    {
        if (section.type == type)
        {
            return section.value.get();
        }
    }
    throw SettingsError(std::string("no settings section is registered for ") + type.name());
}

void load_settings(const ApplicationCommandLineArgs& args)
{
    for (int32_t i = 0; i < args.count; ++i)
    {
        std::string_view arg = args[i];
        if (arg.substr(0, kSettingsFlag.size()) == kSettingsFlag)
        {
            apply(std::filesystem::absolute(std::string(arg.substr(kSettingsFlag.size()))), true);
            return;
        }
    }

    for (const std::filesystem::path& candidate : state().default_files)
    {
        std::filesystem::path file = std::filesystem::absolute(candidate);
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error))
        {
            apply(file, false);
            return;
        }
    }
    apply(std::filesystem::absolute(kDefaultFileName), false);
}

void register_default_settings_file(std::filesystem::path file)
{
    state().default_files.push_back(std::move(file));
}

void reload_settings()
{
    State& current = state();
    if (!current.file.empty())
    {
        apply(current.file, current.file_required);
    }
}

void reset_settings()
{
    State& current = state();
    for (const Section& section : current.sections)
    {
        SharedPtr<void> defaults = section.build(nullptr);
        section.assign(section.value.get(), defaults.get());
    }
    current.file.clear();
    current.file_required = false;
    current.default_files.clear();
}

} // namespace oryx
