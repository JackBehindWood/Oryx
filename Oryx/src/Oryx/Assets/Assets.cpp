#include "oxpch.h"
#include "Oryx/Assets/Assets.h"

#include "Oryx/Core/Application.h"

namespace oryx
{

namespace
{

UniquePtr<AssetManager>& manager_storage()
{
    static UniquePtr<AssetManager> storage;
    return storage;
}

void register_teardown_once()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_shutdown_hook([] { manager_storage().reset(); });
    }
}

} // namespace

AssetManager& Assets::manager()
{
    UniquePtr<AssetManager>& storage = manager_storage();
    if (storage == nullptr)
    {
        if (settings_of<AssetSettings>().worker_threads > 0)
        {
            throw Error("asset worker threads are not implemented", "set assets.worker_threads to 0");
        }
        storage = create_unique<AssetManager>();
        register_teardown_once();
    }
    return *storage;
}

void Assets::install(UniquePtr<AssetManager> manager)
{
    UniquePtr<AssetManager>& storage = manager_storage();
    if (storage != nullptr && storage->live_assets() > 0)
    {
        throw Error("cannot replace the asset manager while assets are live");
    }
    storage = std::move(manager);
    register_teardown_once();
}

void Assets::reset()
{
    manager_storage().reset();
}

void Assets::update()
{
    if (manager_storage() != nullptr)
    {
        manager_storage()->update();
    }
}

} // namespace oryx
