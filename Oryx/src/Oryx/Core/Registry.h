#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Params.h"
#include "Oryx/Containers/FlatHashMap.h"

namespace oryx
{

struct EntryInfo
{
    ParamSchema schema;
    std::string description;
};

template<typename T>
class Registry
{
public:
    using Factory = std::function<UniquePtr<T>(const Params&)>;

    // Not thread-safe (backed by FlatHashMap) - registration is expected to happen single-threaded,
    // via self-registering static init, before any concurrent use.
    static void register_factory(const std::string& name, Factory factory, EntryInfo info = {})
    {
        entries().insert_or_assign(name, Entry{ std::move(factory), std::move(info) });
    }

    // Returns whether the name was registered.
    static bool unregister_factory(const std::string& name)
    {
        return entries().erase(name);
    }

    [[nodiscard]] static UniquePtr<T> create(const std::string& name, const Params& params = {})
    {
        const Entry* entry = entries().find(name);
        if (entry == nullptr)
        {
            return nullptr;
        }
        // A copy, because a factory may register entries and rehash the map that holds it.
        Factory factory = entry->factory;
        return factory(resolve_params(name, entry->info.schema, params));
    }

    [[nodiscard]] static bool has(const std::string& name)
    {
        return entries().find(name) != nullptr;
    }

    [[nodiscard]] static const EntryInfo* info(const std::string& name)
    {
        const Entry* entry = entries().find(name);
        return entry == nullptr ? nullptr : &entry->info;
    }

    // Unordered - callers that need a stable order (e.g. a menu) sort it themselves.
    static std::vector<std::string> names()
    {
        std::vector<std::string> result;
        result.reserve(entries().size());
        entries().for_each([&result](const std::string& name, const Entry&) { result.push_back(name); });
        return result;
    }

private:
    struct Entry
    {
        Factory factory;
        EntryInfo info;
    };

    // Function-local static: avoids static-init-order issues across TUs.
    static FlatHashMap<std::string, Entry, 16>& entries()
    {
        static FlatHashMap<std::string, Entry, 16> instance;
        return instance;
    }
};

template<typename T>
class Register
{
public:
    Register(const std::string& name, typename Registry<T>::Factory factory, EntryInfo info = {})
    {
        Registry<T>::register_factory(name, std::move(factory), std::move(info));
    }
};

template<typename Interface, typename Concrete>
UniquePtr<Interface> construct_from_params([[maybe_unused]] const Params& params)
{
    if constexpr (std::is_constructible_v<Concrete, const Params&>)
    {
        return create_unique<Concrete>(params);
    }
    else
    {
        return create_unique<Concrete>();
    }
}

} // namespace oryx

#define OX_REGISTER_FACTORY(InterfaceType, ConcreteType, name, ...)                                    \
    namespace                                                                                          \
    {                                                                                                  \
    [[maybe_unused]] const ::oryx::Register<InterfaceType>                                             \
        OX_CONCAT(g_ox_register_, __LINE__)(                                                           \
            name,                                                                                      \
            [](const ::oryx::Params& params) -> ::oryx::UniquePtr<InterfaceType>                       \
            { return ::oryx::construct_from_params<InterfaceType, ConcreteType>(params); }             \
            __VA_OPT__(, ::oryx::EntryInfo{ __VA_ARGS__ }));                                           \
    }

#define OX_REGISTER_GAME(Type, name, ...) \
    OX_REGISTER_FACTORY(::oryx::IGame, Type, name __VA_OPT__(,) __VA_ARGS__)
#define OX_REGISTER_STRATEGY(Type, name, ...) \
    OX_REGISTER_FACTORY(::oryx::IStrategy, Type, name __VA_OPT__(,) __VA_ARGS__)
