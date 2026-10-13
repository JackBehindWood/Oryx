#include "oxpch.h"
#include "Oryx/Core/Settings.h"

#include "Oryx/Core/CommandLine.h"

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

std::string_view SettingsNode::section() const
{
    return m_impl->section;
}

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
    std::filesystem::path result(text.empty() ? fallback : std::filesystem::path(text));
    if (result.empty())
    {
        return result;
    }
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

constexpr const char* k_settings_option = "settings";
constexpr const char* k_default_file_name = "oryx.yaml";
constexpr const char* k_override_file_name = "oryx.local.yaml";

struct Section
{
    std::string name;
    std::type_index type;
    SharedPtr<void> value;
    SettingsBuilder build;
    SettingsAssigner assign;
    SettingsWalker walk;
    SettingsCloner clone;
    mutable uint64_t version = 1;
    mutable SharedPtr<void> persisted;
};

struct Subscriber
{
    uint64_t id;
    std::type_index type;
    std::function<void(const void*)> callback;
};

struct State
{
    std::vector<Section> sections;
    std::vector<Subscriber> subscribers;
    uint64_t next_subscriber = 1;
    std::filesystem::path file;
    bool file_required = false;
    std::vector<std::filesystem::path> default_files;
    YAML::Node base_root;
};

State& state()
{
    static State instance;
    return instance;
}

// Callbacks may unsubscribe while running, so each call goes through a copy; one that throws is logged and the rest still run.
void notify(const Section& section)
{
    ++section.version;
    std::vector<Subscriber> pending;
    for (const Subscriber& subscriber : state().subscribers)
    {
        if (subscriber.type == section.type)
        {
            pending.push_back(subscriber);
        }
    }
    for (const Subscriber& subscriber : pending)
    {
        try
        {
            subscriber.callback(section.value.get());
        }
        catch (const Error& error)
        {
            error.log();
        }
    }
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

    const YAML::Node base = root.IsDefined() ? YAML::Clone(root) : YAML::Node();
    YAML::Node overrides;
    if (read_root(file.parent_path() / k_override_file_name, false, overrides) && overrides.IsMap())
    {
        if (!root.IsMap())
        {
            root = YAML::Node(YAML::NodeType::Map);
        }
        for (const auto& section : overrides)
        {
            if (section.second.IsMap())
            {
                for (const auto& key : section.second)
                {
                    root[section.first.as<std::string>()][key.first.as<std::string>()] = key.second;
                }
            }
        }
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
        Section& section = current.sections[i];
        section.assign(section.value.get(), staged[i].get());
        if (section.clone != nullptr)
        {
            section.persisted = section.clone(section.value.get());
        }
    }
    current.file = file;
    current.file_required = required;
    current.base_root = base;
    for (const Section& section : current.sections)
    {
        notify(section);
    }
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

void register_settings_section(std::string name, std::type_index type, SharedPtr<void> value, SettingsBuilder build, SettingsAssigner assign, SettingsWalker walk, SettingsCloner clone)
{
    state().sections.push_back(Section{ std::move(name), type, std::move(value), std::move(build), assign, walk, clone });
}

namespace
{

const Section& section_of(std::type_index type)
{
    for (const Section& section : state().sections)
    {
        if (section.type == type)
        {
            return section;
        }
    }
    throw SettingsError(std::string("no settings section is registered for ") + type.name());
}

} // namespace

const void* settings_value(std::type_index type)
{
    return section_of(type).value.get();
}

void assign_settings(std::type_index type, void* edited)
{
    const Section& section = section_of(type);
    section.assign(section.value.get(), edited);
    notify(section);
}

uint64_t subscribe_settings(std::type_index type, std::function<void(const void*)> callback)
{
    (void)section_of(type);
    State& current = state();
    const uint64_t id = current.next_subscriber++;
    current.subscribers.push_back(Subscriber{ id, type, std::move(callback) });
    return id;
}

void unsubscribe_settings(uint64_t id)
{
    std::vector<Subscriber>& subscribers = state().subscribers;
    subscribers.erase(std::remove_if(subscribers.begin(), subscribers.end(), [id](const Subscriber& subscriber) { return subscriber.id == id; }), subscribers.end());
}

SettingsSubscription::SettingsSubscription(uint64_t id)
    : m_id(id)
{
}

SettingsSubscription::SettingsSubscription(SettingsSubscription&& other) noexcept
    : m_id(std::exchange(other.m_id, 0))
{
}

SettingsSubscription& SettingsSubscription::operator=(SettingsSubscription&& other) noexcept
{
    if (this != &other)
    {
        reset();
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

SettingsSubscription::~SettingsSubscription()
{
    reset();
}

void SettingsSubscription::reset()
{
    if (m_id != 0)
    {
        unsubscribe_settings(m_id);
        m_id = 0;
    }
}

void load_settings(const ApplicationCommandLineArgs& args)
{
    load_settings(CommandLine::global().parse(args));
}

void load_settings(const ParsedArgs& args)
{
    if (args.has(k_settings_option))
    {
        apply(std::filesystem::absolute(args.value(k_settings_option)), true);
        return;
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
    apply(std::filesystem::absolute(k_default_file_name), false);
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
        if (section.clone != nullptr)
        {
            section.persisted = section.clone(section.value.get());
        }
    }
    current.file.clear();
    current.file_required = false;
    current.default_files.clear();
    for (const Section& section : current.sections)
    {
        notify(section);
    }
}

namespace
{

const Section& reflected_section(std::string_view name)
{
    for (const Section& section : state().sections)
    {
        if (section.name == name)
        {
            if (section.walk == nullptr)
            {
                throw SettingsError("settings section '" + std::string(name) + "' has no reflect description");
            }
            return section;
        }
    }
    throw SettingsError("no settings section named '" + std::string(name) + "'");
}

} // namespace

std::vector<std::string> settings_sections()
{
    std::vector<std::string> names;
    for (const Section& section : state().sections)
    {
        if (section.walk != nullptr)
        {
            names.push_back(section.name);
        }
    }
    return names;
}

SharedPtr<void> settings_copy(std::string_view section)
{
    const Section& found = reflected_section(section);
    return found.clone(found.value.get());
}

SharedPtr<void> settings_defaults(std::string_view section)
{
    return reflected_section(section).build(nullptr);
}

void walk_settings(std::string_view section, void* value, ISettingsSink& sink)
{
    reflected_section(section).walk(value, sink);
}

void commit_settings(std::string_view section, void* value)
{
    const Section& found = reflected_section(section);
    found.assign(found.value.get(), value);
    notify(found);
}

SharedPtr<void> settings_persisted(std::string_view section)
{
    const Section& found = reflected_section(section);
    return found.clone(found.persisted != nullptr ? found.persisted.get() : found.value.get());
}

uint64_t settings_version(std::string_view section)
{
    return reflected_section(section).version;
}

std::filesystem::path settings_override_file()
{
    const State& current = state();
    const std::filesystem::path directory = current.file.empty() ? std::filesystem::absolute(".") : current.file.parent_path();
    return directory / k_override_file_name;
}

void save_settings()
{
    State& current = state();
    const std::filesystem::path file = settings_override_file();
    YAML::Node root;
    read_root(file, false, root);
    if (!root.IsMap())
    {
        root = YAML::Node(YAML::NodeType::Map);
    }

    for (const Section& section : current.sections)
    {
        if (section.walk == nullptr)
        {
            continue;
        }
        YAML::Node base_node = current.base_root.IsMap() ? current.base_root[section.name] : YAML::Node();
        SharedPtr<void> base;
        if (base_node.IsDefined() && base_node.IsMap())
        {
            SettingsNode reader = SettingsAccess::make(base_node, current.file, section.name);
            base = section.build(&reader);
        }
        else
        {
            base = section.build(nullptr);
        }
        SharedPtr<void> live = section.clone(section.value.get());
        SettingsText was;
        SettingsText now;
        section.walk(base.get(), was);
        section.walk(live.get(), now);

        YAML::Node changed(YAML::NodeType::Map);
        for (size_t i = 0; i < now.entries().size(); ++i)
        {
            const SettingsText::Entry& entry = now.entries()[i];
            if (entry.text == was.entries()[i].text)
            {
                continue;
            }
            if (!entry.list)
            {
                changed[entry.key] = entry.text;
                continue;
            }
            YAML::Node items(YAML::NodeType::Sequence);
            std::stringstream stream(entry.text);
            for (std::string item; std::getline(stream, item, ',');)
            {
                items.push_back(item);
            }
            changed[entry.key] = items;
        }
        if (changed.size() == 0)
        {
            root.remove(section.name);
        }
        else
        {
            root[section.name] = changed;
        }
    }

    YAML::Emitter emitter;
    emitter << root;
    const std::filesystem::path temporary = file.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << "# Written by Save in the settings panel; delete a key to fall back to the settings file.\n" << emitter.c_str() << "\n";
        if (!out)
        {
            throw SettingsError("cannot write " + temporary.string());
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, file, error);
    if (error)
    {
        throw SettingsError("cannot write " + file.string() + ": " + error.message());
    }
    for (const Section& section : current.sections)
    {
        if (section.clone != nullptr)
        {
            section.persisted = section.clone(section.value.get());
        }
    }
    OX_CORE_INFO("Settings: saved overrides to {}", file.string());
}

namespace
{

class SettingsCommandLine : public ICommandLineContributor
{
public:
    void declare(CommandLine& command_line) const override
    {
        command_line.option(k_settings_option, "FILE", "Settings file to load instead of the default oryx.yaml");
    }
};

OX_REGISTER_COMMAND_LINE(SettingsCommandLine, "settings")

} // namespace

} // namespace oryx
