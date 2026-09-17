#pragma once

#include "Oryx/Core/Base.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace oryx
{

template<typename T>
class Registry
{
public:
    using Factory = std::function<UniquePtr<T>()>;

    static void register_factory(const std::string& name, Factory factory)
    {
        entries()[name] = std::move(factory);
    }

    [[nodiscard]] static UniquePtr<T> create(const std::string& name)
    {
        auto it = entries().find(name);
        if (it == entries().end())
        {
            return nullptr;
        }
        return it->second();
    }

    [[nodiscard]] static bool has(const std::string& name)
    {
        return entries().find(name) != entries().end();
    }

    static std::vector<std::string> names()
    {
        std::vector<std::string> result;
        result.reserve(entries().size());
        for (const auto& [entry_name, factory] : entries())
        {
            result.push_back(entry_name);
        }
        return result;
    }

private:
    // Function-local static: avoids static-init-order issues across TUs.
    static std::unordered_map<std::string, Factory>& entries()
    {
        static std::unordered_map<std::string, Factory> instance;
        return instance;
    }
};

template<typename T>
class Register
{
public:
    Register(const std::string& name, typename Registry<T>::Factory factory)
    {
        Registry<T>::register_factory(name, std::move(factory));
    }
};

} // namespace oryx

#define OX_REGISTER_FACTORY(InterfaceType, ConcreteType, name)                \
    namespace                                                                 \
    {                                                                         \
    [[maybe_unused]] const ::oryx::Register<InterfaceType>                    \
        OX_CONCAT(g_ox_register_, __LINE__)(                                  \
            name, []() -> ::oryx::UniquePtr<InterfaceType>                    \
            { return ::oryx::create_unique<ConcreteType>(); });               \
    }

#define OX_REGISTER_GAME(Type, name) OX_REGISTER_FACTORY(::oryx::IGame, Type, name)
#define OX_REGISTER_STRATEGY(Type, name) OX_REGISTER_FACTORY(::oryx::IStrategy, Type, name)
