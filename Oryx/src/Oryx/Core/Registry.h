#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Containers/FlatHashMap.h"

namespace oryx
{

template<typename T>
class Registry
{
public:
    using Factory = std::function<UniquePtr<T>()>;

    // Not thread-safe (backed by FlatHashMap) - registration is expected to happen single-threaded,
    // via self-registering static init, before any concurrent use.
    static void register_factory(const std::string& name, Factory factory)
    {
        entries().insert_or_assign(name, std::move(factory));
    }

    [[nodiscard]] static UniquePtr<T> create(const std::string& name)
    {
        Factory* factory = entries().find(name);
        if (factory == nullptr)
        {
            return nullptr;
        }
        return (*factory)();
    }

    [[nodiscard]] static bool has(const std::string& name)
    {
        return entries().find(name) != nullptr;
    }

    // Unordered - callers that need a stable order (e.g. a menu) sort it themselves.
    static std::vector<std::string> names()
    {
        std::vector<std::string> result;
        result.reserve(entries().size());
        entries().for_each([&result](const std::string& name, const Factory&) { result.push_back(name); });
        return result;
    }

private:
    // Function-local static: avoids static-init-order issues across TUs.
    static FlatHashMap<std::string, Factory, 16>& entries()
    {
        static FlatHashMap<std::string, Factory, 16> instance;
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
