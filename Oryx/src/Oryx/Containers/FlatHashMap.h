#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Containers/Pair.h"

namespace oryx
{

// Open-addressing (linear probing) map with N inline slots; grows to the heap past a 0.7 load factor. N must be a power of two.
// Not thread-safe: insert_or_assign()'s rehash reallocates and moves every entry with no synchronization.
template<typename Key, typename Value, size_t N>
class FlatHashMap
{
    static_assert(N > 0 && (N & (N - 1)) == 0, "FlatHashMap capacity must be a power of two");

public:
    FlatHashMap() = default;

    FlatHashMap(const FlatHashMap&) = delete;
    FlatHashMap& operator=(const FlatHashMap&) = delete;

    ~FlatHashMap()
    {
        destroy_all();
        free_heap();
    }

    void insert_or_assign(const Key& key, Value value)
    {
        maybe_grow();
        size_t index = probe_for(key);
        if (m_occupied[index])
        {
            m_slots[index].value = std::move(value);
        }
        else
        {
            new (&m_slots[index]) Pair<Key, Value>{ key, std::move(value) };
            m_occupied[index] = true;
            ++m_count;
        }
    }

    // Backward-shift deletion keeps every remaining key reachable from its home slot without tombstones; returns whether the key existed.
    bool erase(const Key& key)
    {
        size_t hole = probe_for(key);
        if (!m_occupied[hole])
        {
            return false;
        }

        m_slots[hole].~Pair<Key, Value>();
        m_occupied[hole] = false;
        --m_count;

        size_t mask = m_capacity - 1;
        for (size_t next = (hole + 1) & mask; m_occupied[next]; next = (next + 1) & mask)
        {
            size_t home = std::hash<Key>{}(m_slots[next].key) & mask;
            bool reachable_past_hole = hole <= next ? (hole < home && home <= next) : (hole < home || home <= next);
            if (reachable_past_hole)
            {
                continue;
            }

            new (&m_slots[hole]) Pair<Key, Value>(std::move(m_slots[next]));
            m_slots[next].~Pair<Key, Value>();
            m_occupied[hole] = true;
            m_occupied[next] = false;
            hole = next;
        }
        return true;
    }

    [[nodiscard]] Value* find(const Key& key)
    {
        size_t index = probe_for(key);
        return m_occupied[index] ? &m_slots[index].value : nullptr;
    }

    [[nodiscard]] const Value* find(const Key& key) const
    {
        size_t index = probe_for(key);
        return m_occupied[index] ? &m_slots[index].value : nullptr;
    }

    [[nodiscard]] size_t size() const { return m_count; }
    [[nodiscard]] bool empty() const { return m_count == 0; }

    // Visits every entry as fn(key, value), unordered.
    template<typename Fn>
    void for_each(Fn&& fn) const
    {
        for (size_t i = 0; i < m_capacity; ++i)
        {
            if (m_occupied[i])
            {
                fn(m_slots[i].key, m_slots[i].value);
            }
        }
    }

private:
    size_t probe_for(const Key& key) const
    {
        size_t index = std::hash<Key>{}(key) & (m_capacity - 1);
        while (m_occupied[index] && !(m_slots[index].key == key))
        {
            index = (index + 1) & (m_capacity - 1);
        }
        return index;
    }

    void maybe_grow()
    {
        if ((m_count + 1) * 10 > m_capacity * 7) // load factor > 0.7
        {
            rehash(m_capacity * 2);
        }
    }

    void rehash(size_t new_capacity)
    {
        Pair<Key, Value>* new_slots =
            static_cast<Pair<Key, Value>*>(::operator new(new_capacity * sizeof(Pair<Key, Value>)));
        bool* new_occupied = new bool[new_capacity]();

        Pair<Key, Value>* old_slots = m_slots;
        bool* old_occupied = m_occupied;
        size_t old_capacity = m_capacity;
        bool old_was_heap = m_is_heap;

        m_slots = new_slots;
        m_occupied = new_occupied;
        m_capacity = new_capacity;
        m_is_heap = true;
        m_count = 0;

        for (size_t i = 0; i < old_capacity; ++i)
        {
            if (old_occupied[i])
            {
                insert_or_assign(old_slots[i].key, std::move(old_slots[i].value));
                old_slots[i].~Pair<Key, Value>();
            }
        }

        if (old_was_heap)
        {
            ::operator delete(old_slots);
            delete[] old_occupied;
        }
    }

    void destroy_all()
    {
        for (size_t i = 0; i < m_capacity; ++i)
        {
            if (m_occupied[i])
            {
                m_slots[i].~Pair<Key, Value>();
            }
        }
    }

    void free_heap()
    {
        if (m_is_heap)
        {
            ::operator delete(m_slots);
            delete[] m_occupied;
        }
    }

    alignas(Pair<Key, Value>) unsigned char m_inline_slots[N * sizeof(Pair<Key, Value>)];
    bool m_inline_occupied[N] = {};

    Pair<Key, Value>* m_slots = reinterpret_cast<Pair<Key, Value>*>(m_inline_slots);
    bool* m_occupied = m_inline_occupied;
    size_t m_capacity = N;
    size_t m_count = 0;
    bool m_is_heap = false;
};

} // namespace oryx
