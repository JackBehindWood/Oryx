#pragma once

#include "Oryx/Assets/Asset.h"
#include "Oryx/Assets/AssetSettings.h"
#include "Oryx/Assets/AssetTable.h"
#include "Oryx/Assets/Execution/IAssetExecutor.h"
#include "Oryx/Assets/Import/CompiledAssetStore.h"
#include "Oryx/Assets/Import/IAssetImporter.h"
#include "Oryx/Assets/AssetFile.h"
#include "Oryx/Core/ResourceSettings.h"

namespace oryx
{

template<typename T>
class ImportRequest;

// Not thread-safe: every public call and every publish happens on the main thread.
// load and reload only queue work; Assets::update() (or wait) runs it and settles the slot.
class AssetManager
{
public:
    explicit AssetManager(const AssetSettings& settings = settings_of<AssetSettings>(), const ResourceSettings& resources = settings_of<ResourceSettings>(), UniquePtr<IAssetExecutor> executor = nullptr);
    virtual ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    template<typename T>
    AssetHandle<T> load(const std::filesystem::path& path)
    {
        static_assert(std::is_base_of_v<Asset, T>, "managed assets derive from Asset");
        std::string key = path_key(path);
        detail::AssetTable<T>& table = table_for<T>();
        AssetHandle<T> existing = table.find(key);
        if (!is_null(existing))
        {
            ++table.slot(existing).ref_count;
            return existing;
        }

        AssetHandle<T> handle = table.acquire_slot(key);
        submit<T>(handle, path, false);
        return handle;
    }

    template<typename T>
    AssetHandle<T> load_blocking(const std::filesystem::path& path)
    {
        AssetHandle<T> handle = load<T>(path);
        wait(handle);
        return handle;
    }

    // The old asset keeps being served until the new one is ready; a failure keeps it and records error().
    template<typename T>
    void reload(AssetHandle<T> handle)
    {
        typename detail::AssetTable<T>::Slot& slot = table_for<T>().slot(handle);
        if (slot.pending == nullptr)
        {
            submit<T>(handle, std::filesystem::path(slot.path), true);
        }
    }

    // Pumps until no request for the handle is pending.
    template<typename T>
    void wait(AssetHandle<T> handle)
    {
        while (table_for<T>().slot(handle).pending != nullptr)
        {
            if (m_executor->idle())
            {
                throw Error("asset request was lost", table_for<T>().slot(handle).path);
            }
            drain(AssetBudget{});
        }
    }

    // Runs on the main thread from update() once nothing is pending for the handle; runs on the next update when it is already settled.
    template<typename T>
    void on_ready(AssetHandle<T> handle, std::function<void(AssetHandle<T>)> callback)
    {
        typename detail::AssetTable<T>::Slot& slot = table_for<T>().slot(handle);
        if (slot.pending != nullptr)
        {
            slot.on_ready.push_back(std::move(callback));
            return;
        }
        m_callbacks.push_back([handle, callback = std::move(callback)] { callback(handle); });
    }

    void update() { drain(m_budget); }
    void set_budget(const AssetBudget& budget) { m_budget = budget; }

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
            cancel(slot.pending);
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
    [[nodiscard]] bool pending(AssetHandle<T> handle) const
    {
        return checked_slot(handle).pending != nullptr;
    }

    // Bumped each time a reload swaps in a new asset; the handle itself stays valid.
    template<typename T>
    [[nodiscard]] uint32_t revision(AssetHandle<T> handle) const
    {
        return checked_slot(handle).revision;
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

    [[nodiscard]] size_t live_assets() const;
    // The compiled-asset store, shared with consumers that compile their own output (the glyph atlas).
    [[nodiscard]] const CompiledAssetStore& compiled() const { return m_compiled; }

protected:
    // Maps a requested path to the file to read: roots are searched in order, then the path is used as given.
    [[nodiscard]] virtual std::filesystem::path resolve_path(const std::filesystem::path& path) const;
    // Main thread, right after a reload swapped in a new asset.
    virtual void on_reloaded(std::type_index type, AssetId id);

private:
    template<typename U>
    friend class ImportRequest;

    [[nodiscard]] static std::string path_key(const std::filesystem::path& path);
    [[nodiscard]] static std::string extension_key(const std::filesystem::path& path);
    [[nodiscard]] static std::string describe(const Error& error);

    void drain(const AssetBudget& budget);
    void cancel(AssetRequest* request);

    template<typename T>
    void submit(AssetHandle<T> handle, const std::filesystem::path& path, bool is_reload)
    {
        UniquePtr<ImportRequest<T>> request = create_unique<ImportRequest<T>>(*this, handle, resolve_path(path), is_reload);
        table_for<T>().slot(handle).pending = request.get();
        m_executor->submit(*request);
        AssetRequest* key = request.get();
        m_requests.emplace(key, std::move(request));
    }

    template<typename T>
    void publish_import(ImportRequest<T>& request)
    {
        if (request.cancelled || !table_for<T>().alive(request.handle))
        {
            return;
        }
        UniquePtr<T> asset;
        std::string failure = request.error;
        if (failure.empty())
        {
            try
            {
                asset = request.importer->instantiate(request.payload.data(), request.payload.size());
            }
            catch (const Error& e)
            {
                failure = describe(e);
            }
            if (failure.empty() && asset == nullptr)
            {
                failure = "importer returned no asset";
            }
        }

        // instantiate may load assets and grow the table, so the slot is looked up again.
        detail::AssetTable<T>& table = table_for<T>();
        if (!table.alive(request.handle))
        {
            return;
        }
        typename detail::AssetTable<T>::Slot& slot = table.slot(request.handle);
        slot.pending = nullptr;
        bool swapped = failure.empty();
        if (swapped)
        {
            asset->m_source_path = request.source();
            slot.asset = std::move(asset);
            slot.state = AssetState::Ready;
            slot.error.clear();
            if (request.is_reload)
            {
                ++slot.revision;
            }
        }
        else
        {
            slot.error = std::move(failure);
            if (!request.is_reload)
            {
                slot.state = AssetState::Failed;
                slot.asset = nullptr;
            }
        }
        AssetHandle<T> handle = request.handle;
        for (std::function<void(AssetHandle<T>)>& callback : slot.on_ready)
        {
            m_callbacks.push_back([handle, callback = std::move(callback)] { callback(handle); });
        }
        slot.on_ready.clear();
        if (swapped && request.is_reload)
        {
            on_reloaded(std::type_index(typeid(T)), handle.id);
        }
    }

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

    AssetSettings m_settings;
    ResourceSettings m_resources;
    CompiledAssetStore m_compiled;
    UniquePtr<IAssetExecutor> m_executor;
    AssetBudget m_budget;
    std::unordered_map<std::type_index, UniquePtr<detail::AssetTableBase>> m_tables;
    std::unordered_map<AssetRequest*, UniquePtr<AssetRequest>> m_requests;
    std::vector<std::function<void()>> m_callbacks;
};

template<typename T>
class ImportRequest final : public AssetRequest
{
public:
    ImportRequest(AssetManager& manager, AssetHandle<T> handle, std::filesystem::path source, bool is_reload)
        : handle(handle)
        , is_reload(is_reload)
        , m_manager(manager)
        , m_source(std::move(source))
    {
    }

    void process() override
    {
        try
        {
            std::string extension = AssetManager::extension_key(m_source);
            importer = Registry<IAssetImporter<T>>::create(extension);
            if (importer == nullptr)
            {
                error = "no importer registered for '" + extension + "'";
                return;
            }
            std::vector<uint8_t> bytes = read_binary_file(m_source);
            CompiledAssetKey key = make_compiled_asset_key(importer->id(), importer->version(), importer->settings_hash(), bytes.data(), bytes.size());
            if (!m_manager.m_compiled.read(key, payload))
            {
                payload = importer->import_source(ImportInput{ m_source, bytes.data(), bytes.size() });
                m_manager.m_compiled.write(key, payload.data(), payload.size());
            }
        }
        catch (const Error& e)
        {
            error = AssetManager::describe(e);
        }
    }

    [[nodiscard]] const std::filesystem::path& source() const { return m_source; }

    void publish() override { m_manager.publish_import(*this); }

    AssetHandle<T> handle;
    bool is_reload;
    UniquePtr<IAssetImporter<T>> importer;
    std::vector<uint8_t> payload;
    std::string error;

private:
    AssetManager& m_manager;
    std::filesystem::path m_source;
};

} // namespace oryx
