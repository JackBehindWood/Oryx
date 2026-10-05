#pragma once

#include "Oryx/Assets/AssetManager.h"

namespace oryx
{

// Global access to the application's AssetManager, created on first use from the `assets:` settings.
// An application swaps in its own subclass with use<T>() before it loads anything.
class Assets
{
public:
    [[nodiscard]] static AssetManager& manager();

    // Throws Error when the current manager still holds live assets.
    template<typename T, typename... Args>
    static T& use(Args&&... args)
    {
        static_assert(std::is_base_of_v<AssetManager, T>);
        UniquePtr<T> created = create_unique<T>(std::forward<Args>(args)...);
        T& installed = *created;
        install(std::move(created));
        return installed;
    }

    template<typename T>
    [[nodiscard]] static T& manager_as()
    {
        T* typed = dynamic_cast<T*>(&manager());
        if (typed == nullptr)
        {
            throw Error("the asset manager is not of the requested type");
        }
        return *typed;
    }

    // Destroys the current manager; the next manager() creates a default one again.
    static void reset();

    // No-op before the first use of the manager, since nothing can be queued yet.
    static void update();

    template<typename T>
    static AssetHandle<T> load(const std::filesystem::path& path) { return manager().load<T>(path); }
    template<typename T>
    static AssetHandle<T> load_blocking(const std::filesystem::path& path) { return manager().load_blocking<T>(path); }
    template<typename T>
    static void reload(AssetHandle<T> handle) { manager().reload(handle); }
    template<typename T>
    static void release(AssetHandle<T> handle) { manager().release(handle); }
    template<typename T>
    static void wait(AssetHandle<T> handle) { manager().wait(handle); }
    template<typename T>
    static void on_ready(AssetHandle<T> handle, std::function<void(AssetHandle<T>)> callback) { manager().on_ready(handle, std::move(callback)); }
    template<typename T>
    [[nodiscard]] static AssetState state(AssetHandle<T> handle) { return manager().state(handle); }
    template<typename T>
    [[nodiscard]] static uint32_t revision(AssetHandle<T> handle) { return manager().revision(handle); }
    template<typename T>
    [[nodiscard]] static const T& get(AssetHandle<T> handle) { return manager().get(handle); }
    template<typename T>
    [[nodiscard]] static const T* try_get(AssetHandle<T> handle) { return manager().try_get(handle); }

private:
    static void install(UniquePtr<AssetManager> manager);
};

} // namespace oryx
