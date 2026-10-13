#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/CommandLine.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Core/SettingsReflect.h"

namespace oryx
{

struct ApplicationCommandLineArgs;

class SettingsError : public Error
{
public:
    explicit SettingsError(const std::string& message)
        : Error(message)
    {
    }

    [[nodiscard]] const char* category() const noexcept override { return "settings"; }
};

// One section's mapping from the settings file; a key the section's reader never asks for is reported as unknown once it returns.
class SettingsNode
{
public:
    ~SettingsNode();

    SettingsNode(const SettingsNode&) = delete;
    SettingsNode& operator=(const SettingsNode&) = delete;

    [[nodiscard]] std::string_view section() const;
    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] std::string string(std::string_view key, std::string fallback = "") const;
    [[nodiscard]] int64_t integer(std::string_view key, int64_t fallback = 0) const;
    [[nodiscard]] bool boolean(std::string_view key, bool fallback = false) const;
    [[nodiscard]] std::vector<std::string> strings(std::string_view key) const;

    // A relative path is taken relative to the directory of the settings file.
    [[nodiscard]] std::filesystem::path path(std::string_view key, std::filesystem::path fallback = {}) const;
    [[nodiscard]] std::vector<std::filesystem::path> paths(std::string_view key) const;

private:
    struct Impl;
    friend struct SettingsAccess;

    explicit SettingsNode(UniquePtr<Impl> impl);

    UniquePtr<Impl> m_impl;
};

// Reads --settings=<file> (declared by Settings' own command-line contributor), else the first default file an application registered that exists, else <working directory>/oryx.yaml
// (a missing default file means every section keeps its defaults).
// Throws SettingsError for an unreadable or malformed file or a wrong value, leaving the previous values in place.
void load_settings(const ParsedArgs& args);
// Parses `args` against every registered contributor first; throws Error for an unknown option.
void load_settings(const ApplicationCommandLineArgs& args);

// An application's own settings file, relative to the working directory, tried before oryx.yaml.
void register_default_settings_file(std::filesystem::path file);

// Reads the same file again, then the override file beside it; a failed reload keeps the values that were in effect. Does nothing before a first load.
void reload_settings();

// Overrides are the keys Save wrote because they differ from the loaded file; they sit in oryx.local.yaml next to it and win over it, so a hand-written file is never rewritten.
[[nodiscard]] std::filesystem::path settings_override_file();
// Writes the reflected sections' keys that differ from the loaded file to the override file (other sections already in it are kept); throws SettingsError when it cannot write.
void save_settings();

// Every section back to its defaults, no file remembered and no default file registered.
void reset_settings();

const void* settings_value(std::type_index type);

using SettingsBuilder = std::function<SharedPtr<void>(const SettingsNode*)>;
using SettingsAssigner = void (*)(void* to, void* from);
using SettingsWalker = void (*)(void* value, ISettingsSink& sink);
using SettingsCloner = SharedPtr<void> (*)(const void* value);

// `walk` and `clone` are null for a section that has no reflect description.
void register_settings_section(std::string name, std::type_index type, SharedPtr<void> value, SettingsBuilder build, SettingsAssigner assign, SettingsWalker walk, SettingsCloner clone);

// Sections with a reflect description, in registration order.
[[nodiscard]] std::vector<std::string> settings_sections();
// A private copy of a section's live values (or of its defaults) to edit and walk; throws SettingsError for an unknown or unreflected section.
[[nodiscard]] SharedPtr<void> settings_copy(std::string_view section);
[[nodiscard]] SharedPtr<void> settings_defaults(std::string_view section);
// What the files hold: the values after the last load, reload or Save (or the defaults after a reset). A change since then is unsaved.
[[nodiscard]] SharedPtr<void> settings_persisted(std::string_view section);
void walk_settings(std::string_view section, void* value, ISettingsSink& sink);
// Assigns a copy as the section's live values and notifies; the file is not touched.
void commit_settings(std::string_view section, void* value);
// Grows with every load, reload, reset and update of the section.
[[nodiscard]] uint64_t settings_version(std::string_view section);

// Console-style access by "section.key"; both throw SettingsError for an unknown path or a rejected value.
[[nodiscard]] std::string get_setting(std::string_view path);
void set_setting(std::string_view path, std::string_view text);
// Every field of a section (all sections when empty) as text, with its description.
[[nodiscard]] std::vector<SettingsText::Entry> describe_settings(std::string_view section);

void assign_settings(std::type_index type, void* edited);
[[nodiscard]] uint64_t subscribe_settings(std::type_index type, std::function<void(const void*)> callback);
void unsubscribe_settings(uint64_t id);

// Unsubscribes on destruction; keep it as a member of whatever the callback captures.
class SettingsSubscription
{
public:
    SettingsSubscription() = default;
    explicit SettingsSubscription(uint64_t id);
    SettingsSubscription(SettingsSubscription&& other) noexcept;
    SettingsSubscription& operator=(SettingsSubscription&& other) noexcept;
    SettingsSubscription(const SettingsSubscription&) = delete;
    SettingsSubscription& operator=(const SettingsSubscription&) = delete;
    ~SettingsSubscription();

    void reset();

private:
    uint64_t m_id = 0;
};

// The values of a registered section: its defaults until a settings file has been loaded.
template<typename T>
[[nodiscard]] const T& settings_of()
{
    return *static_cast<const T*>(settings_value(std::type_index(typeid(T))));
}

// Calls `callback` with the section's values after every load, reload, reset and update_settings, changed or not; an Error it throws is logged and does not stop the other callbacks.
template<typename T>
[[nodiscard]] SettingsSubscription on_settings_changed(std::function<void(const T&)> callback)
{
    return SettingsSubscription(subscribe_settings(std::type_index(typeid(T)), [callback = std::move(callback)](const void* value) { callback(*static_cast<const T*>(value)); }));
}

// Edits a copy of the section and assigns it, then notifies; the settings file is not touched, so the next reload_settings restores the file's values.
template<typename T>
void update_settings(const std::function<void(T&)>& edit)
{
    T edited = settings_of<T>();
    edit(edited);
    assign_settings(std::type_index(typeid(T)), &edited);
}

// A section is a default-constructible, assignable, data-only struct plus either `reflect(Visitor&, T&)` (SettingsReflect.h; the panel, console and Save see it) or the older `void read_settings(T&, const SettingsNode&)` next to it.
template<typename T>
class RegisterSettings
{
public:
    static_assert(std::is_default_constructible_v<T> && std::is_move_assignable_v<T>);

    explicit RegisterSettings(std::string name)
    {
        register_settings_section(std::move(name), std::type_index(typeid(T)), create_shared<T>(),
            [](const SettingsNode* node) -> SharedPtr<void>
            {
                SharedPtr<T> value = create_shared<T>();
                if (node != nullptr)
                {
                    if constexpr (SettingsReflected<T>)
                    {
                        SettingsReader reader(*node);
                        SettingsVisitor visitor(reader);
                        reflect(visitor, *value);
                    }
                    else
                    {
                        read_settings(*value, *node);
                    }
                }
                return value;
            },
            [](void* to, void* from) { *static_cast<T*>(to) = std::move(*static_cast<T*>(from)); },
            walker(), cloner());
    }

private:
    static SettingsWalker walker()
    {
        if constexpr (SettingsReflected<T>)
        {
            return [](void* value, ISettingsSink& sink)
            {
                SettingsVisitor visitor(sink);
                reflect(visitor, *static_cast<T*>(value));
            };
        }
        else
        {
            return nullptr;
        }
    }

    static SettingsCloner cloner()
    {
        if constexpr (SettingsReflected<T>)
        {
            return [](const void* value) -> SharedPtr<void> { return create_shared<T>(*static_cast<const T*>(value)); };
        }
        else
        {
            return nullptr;
        }
    }
};

} // namespace oryx

#define OX_REGISTER_DEFAULT_SETTINGS_FILE(file) \
    namespace                                   \
    {                                           \
    [[maybe_unused]] const int OX_CONCAT(g_ox_register_settings_file_, __LINE__) = (::oryx::register_default_settings_file(file), 0); \
    }

#define OX_REGISTER_SETTINGS(Type, name) \
    namespace                            \
    {                                    \
    [[maybe_unused]] const ::oryx::RegisterSettings<Type> OX_CONCAT(g_ox_register_settings_, __LINE__)(name); \
    }
