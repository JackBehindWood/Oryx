#pragma once

#include "Oryx/Assets/AssetTable.h"
#include "Oryx/Assets/IAssetLoader.h"

namespace oryx
{

// Not thread-safe; loading is synchronous.
class AssetManager
{
public:
    template<typename T>
    AssetHandle<T> load(const std::filesystem::path& path)
    {
        std::string key = path_key(path);
        detail::AssetTable<T>& table = table_for<T>();
        AssetHandle<T> existing = table.find(key);
        if (!is_null(existing))
        {
            ++table.slot(existing).ref_count;
            return existing;
        }

        AssetHandle<T> handle = table.acquire_slot(key);
        UniquePtr<T> asset;
        std::string error;
        try
        {
            UniquePtr<IAssetLoader<T>> loader = Registry<IAssetLoader<T>>::create(extension_key(path));
            if (loader == nullptr)
            {
                error = "no loader registered for '" + extension_key(path) + "'";
            }
            else
            {
                asset = loader->load(path);
            }
        }
        catch (const Error& e)
        {
            error = e.what();
            if (!e.detail().empty())
            {
                error += ": " + e.detail();
            }
        }

        detail::AssetTable<T>& settled = table_for<T>();
        typename detail::AssetTable<T>::Slot& slot = settled.slot(handle);
        if (asset == nullptr && error.empty())
        {
            error = "loader returned no asset";
        }
        slot.state = error.empty() ? AssetState::Ready : AssetState::Failed;
        slot.asset = std::move(asset);
        slot.error = std::move(error);
        return handle;
    }

    template<typename T>
    [[nodiscard]] AssetHandle<T> find(const std::filesystem::path& path) const
    {
        const detail::AssetTable<T>* table = existing_table<T>();
        return table == nullptr ? AssetHandle<T>{} : table->find(path_key(path));
    }

    template<typename T>
    void release(AssetHandle<T> handle)
    {
        detail::AssetTable<T>& table = table_for<T>();
        typename detail::AssetTable<T>::Slot& slot = table.slot(handle);
        if (--slot.ref_count == 0)
        {
            table.release_slot(handle);
        }
    }

    template<typename T>
    [[nodiscard]] bool alive(AssetHandle<T> handle) const
    {
        const detail::AssetTable<T>* table = existing_table<T>();
        return table != nullptr && table->alive(handle);
    }

    template<typename T>
    [[nodiscard]] AssetState state(AssetHandle<T> handle) const
    {
        return checked_slot(handle).state;
    }

    template<typename T>
    [[nodiscard]] const std::string& error(AssetHandle<T> handle) const
    {
        return checked_slot(handle).error;
    }

    template<typename T>
    [[nodiscard]] const std::string& path(AssetHandle<T> handle) const
    {
        return checked_slot(handle).path;
    }

    template<typename T>
    [[nodiscard]] uint32_t ref_count(AssetHandle<T> handle) const
    {
        return checked_slot(handle).ref_count;
    }

    template<typename T>
    [[nodiscard]] const T& get(AssetHandle<T> handle) const
    {
        const typename detail::AssetTable<T>::Slot& slot = checked_slot(handle);
        if (slot.state != AssetState::Ready)
        {
            throw Error("asset '" + slot.path + "' is not ready", slot.error);
        }
        return *slot.asset;
    }

    template<typename T>
    [[nodiscard]] const T* try_get(AssetHandle<T> handle) const
    {
        const detail::AssetTable<T>* table = existing_table<T>();
        if (table == nullptr || !table->alive(handle))
        {
            return nullptr;
        }
        const typename detail::AssetTable<T>::Slot& slot = table->slot(handle);
        return slot.state == AssetState::Ready ? slot.asset.get() : nullptr;
    }

    template<typename T>
    [[nodiscard]] size_t size() const
    {
        const detail::AssetTable<T>* table = existing_table<T>();
        return table == nullptr ? 0 : table->live_count();
    }

private:
    [[nodiscard]] static std::string path_key(const std::filesystem::path& path);
    [[nodiscard]] static std::string extension_key(const std::filesystem::path& path);

    template<typename T>
    detail::AssetTable<T>& table_for()
    {
        UniquePtr<detail::AssetTableBase>& entry = m_tables[std::type_index(typeid(T))];
        if (entry == nullptr)
        {
            entry = create_unique<detail::AssetTable<T>>();
        }
        return static_cast<detail::AssetTable<T>&>(*entry);
    }

    template<typename T>
    const detail::AssetTable<T>* existing_table() const
    {
        std::unordered_map<std::type_index, UniquePtr<detail::AssetTableBase>>::const_iterator it = m_tables.find(std::type_index(typeid(T)));
        return it == m_tables.end() ? nullptr : static_cast<const detail::AssetTable<T>*>(it->second.get());
    }

    template<typename T>
    const typename detail::AssetTable<T>::Slot& checked_slot(AssetHandle<T> handle) const
    {
        const detail::AssetTable<T>* table = existing_table<T>();
        if (table == nullptr)
        {
            throw Error("stale or null asset handle");
        }
        return table->slot(handle);
    }

    std::unordered_map<std::type_index, UniquePtr<detail::AssetTableBase>> m_tables;
};

} // namespace oryx
