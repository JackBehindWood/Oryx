#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"

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

// Reads --settings=<file>, else the first default file an application registered that exists, else <working directory>/oryx.yaml
// (a missing default file means every section keeps its defaults).
// Throws SettingsError for an unreadable or malformed file or a wrong value, leaving the previous values in place.
void load_settings(const ApplicationCommandLineArgs& args);

// An application's own settings file, relative to the working directory, tried before oryx.yaml.
void register_default_settings_file(std::filesystem::path file);

// Reads the same file again; a failed reload keeps the values that were in effect. Does nothing before a first load.
void reload_settings();

// Every section back to its defaults, no file remembered and no default file registered.
void reset_settings();

const void* settings_value(std::type_index type);

using SettingsBuilder = std::function<SharedPtr<void>(const SettingsNode*)>;
using SettingsAssigner = void (*)(void* to, void* from);

void register_settings_section(std::string name, std::type_index type, SharedPtr<void> value, SettingsBuilder build, SettingsAssigner assign);

// The values of a registered section: its defaults until a settings file has been loaded.
template<typename T>
[[nodiscard]] const T& settings_of()
{
    return *static_cast<const T*>(settings_value(std::type_index(typeid(T))));
}

// A section is a default-constructible, assignable, data-only struct plus `void read_settings(T&, const SettingsNode&)` next to it.
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
                    read_settings(*value, *node);
                }
                return value;
            },
            [](void* to, void* from) { *static_cast<T*>(to) = std::move(*static_cast<T*>(from)); });
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
