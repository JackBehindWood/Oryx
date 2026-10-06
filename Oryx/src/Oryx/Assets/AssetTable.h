#pragma once

#include "Oryx/Assets/AssetHandle.h"
#include "Oryx/Assets/Execution/AssetRequest.h"
#include "Oryx/Containers/FlatHashMap.h"
#include "Oryx/Core/Error.h"

namespace oryx
{

enum class AssetState : uint8_t
{
    Loading,
    Ready,
    Failed,
};

namespace detail
{

class AssetTableBase
{
public:
    virtual ~AssetTableBase() = default;

    [[nodiscard]] virtual size_t live_count() const = 0;
};

template<typename T>
class AssetTable : public AssetTableBase
{
public:
    struct Slot
    {
        UniquePtr<T> asset;
        uint32_t generation = 1;
        uint32_t revision = 1;
        uint32_t ref_count = 0;
        AssetState state = AssetState::Loading;
        std::string path;
        std::string error;
        AssetRequest* pending = nullptr;
        std::vector<std::function<void(AssetHandle<T>)>> on_ready;
    };

    [[nodiscard]] AssetHandle<T> find(const std::string& key) const
    {
        const uint32_t* index = m_by_path.find(key);
        if (index == nullptr)
        {
            return {};
        }
        return handle_at(*index);
    }

    [[nodiscard]] AssetHandle<T> acquire_slot(const std::string& key)
    {
        uint32_t index = 0;
        if (m_free.empty())
        {
            index = static_cast<uint32_t>(m_slots.size());
            m_slots.emplace_back();
        }
        else
        {
            index = m_free.back();
            m_free.pop_back();
        }
        Slot& slot = m_slots[index];
        slot.ref_count = 1;
        slot.state = AssetState::Loading;
        slot.revision = 1;
        slot.pending = nullptr;
        slot.path = key;
        m_by_path.insert_or_assign(key, index);
        return handle_at(index);
    }

    void release_slot(AssetHandle<T> handle)
    {
        uint32_t index = handle.id.value - 1;
        Slot& slot = m_slots[index];
        m_by_path.erase(slot.path);
        slot.asset = nullptr;
        slot.path.clear();
        slot.error.clear();
        slot.pending = nullptr;
        slot.on_ready.clear();
        slot.ref_count = 0;
        ++slot.generation;
        m_free.push_back(index);
    }

    [[nodiscard]] bool alive(AssetHandle<T> handle) const
    {
        if (is_null(handle) || handle.id.value > m_slots.size())
        {
            return false;
        }
        const Slot& slot = m_slots[handle.id.value - 1];
        return slot.generation == handle.generation && slot.ref_count > 0;
    }

    [[nodiscard]] Slot& slot(AssetHandle<T> handle)
    {
        if (!alive(handle))
        {
            throw Error("stale or null asset handle");
        }
        return m_slots[handle.id.value - 1];
    }

    [[nodiscard]] const Slot& slot(AssetHandle<T> handle) const
    {
        if (!alive(handle))
        {
            throw Error("stale or null asset handle");
        }
        return m_slots[handle.id.value - 1];
    }

    [[nodiscard]] size_t live_count() const override { return m_slots.size() - m_free.size(); }

private:
    [[nodiscard]] AssetHandle<T> handle_at(uint32_t index) const
    {
        return AssetHandle<T>{ AssetId{ index + 1 }, m_slots[index].generation };
    }

    std::vector<Slot> m_slots;
    std::vector<uint32_t> m_free;
    FlatHashMap<std::string, uint32_t, 16> m_by_path;
};

} // namespace detail

} // namespace oryx
