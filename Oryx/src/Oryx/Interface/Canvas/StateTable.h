#pragma once

#include "Oryx/Interface/Canvas/ImId.h"

namespace oryx
{

// Per-id state kept across frames, flat and sorted by id. An entry not touched in a frame is dropped by `collect` at that frame's end, so state of vanished widgets does not pile up.
// Lookups are O(log n); inserts shift the tail but allocate only while the table grows.
template<typename T>
class StateTable
{
public:
    // The entry for `id`, created default-initialised if absent, marked as used in `frame`.
    T& get(ImId id, uint64_t frame)
    {
        auto at = std::lower_bound(m_entries.begin(), m_entries.end(), id.value, [](const Entry& entry, uint64_t value) { return entry.id < value; });
        if (at == m_entries.end() || at->id != id.value)
        {
            at = m_entries.insert(at, Entry{ id.value, frame, T{} });
        }
        at->frame = frame;
        return at->value;
    }

    // Null when absent. Does not mark the entry used.
    [[nodiscard]] const T* find(ImId id) const
    {
        auto at = std::lower_bound(m_entries.begin(), m_entries.end(), id.value, [](const Entry& entry, uint64_t value) { return entry.id < value; });
        return at != m_entries.end() && at->id == id.value ? &at->value : nullptr;
    }

    // Null when absent. Does not mark the entry used.
    [[nodiscard]] T* find_mut(ImId id)
    {
        return const_cast<T*>(std::as_const(*this).find(id));
    }

    // Drops entries last used before `frame`.
    void collect(uint64_t frame)
    {
        m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [frame](const Entry& entry) { return entry.frame < frame; }), m_entries.end());
    }

    void clear() { m_entries.clear(); }
    [[nodiscard]] size_t size() const { return m_entries.size(); }

private:
    struct Entry
    {
        uint64_t id;
        uint64_t frame;
        T value;
    };

    std::vector<Entry> m_entries;
};

} // namespace oryx
